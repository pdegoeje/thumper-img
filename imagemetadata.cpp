#include "imagemetadata.h"
#include "imagedao.h"

#include "sqlitehelper.h"

#include "dct/fast-dct-lee.h"

#include <QImageReader>
#include <QBuffer>
#include <QDebug>
#include <QTextStream>

#include <set>
#include <unordered_map>

uint64_t perceptualHash(const QImage &image) {
  Q_ASSERT(image.width() == 32);
  Q_ASSERT(image.height() == 32);

  double mat[32][32];

  for(int y = 0; y < 32; y++) {
    const uchar *scanLine = image.constScanLine(y);
    for(int x = 0; x < 32; x++) {
      mat[y][x] = (double)scanLine[x];
    }
  }

  // Calculate 2D DCT type II.
  // Note that we don't care about the scaling, as we're going to compare to the mean anyway.

  // Transform each row
  for(int y = 0; y < 32; y++) {
    FastDctLee_transform(mat[y], 32);
  }

  // Transform each column (we only care about the first 8 columns).
  for(int x = 0; x < 8; x++) {
    double col[32];
    for(int y = 0; y < 32; y++) {
      col[y] = mat[y][x];
    }
    FastDctLee_transform(col, 32);
    for(int y = 0; y < 32; y++) {
      mat[y][x] = col[y];
    }
  }

  // Calculate the mean of each cosine amplitude, but exclude the DC component.
  // The lowest 8x8 frequencies are taken.
  double acc = 0;
  for(int i = 1; i < 65; i++) {
    int y = i / 8;
    int x = i % 8;
    acc += mat[y][x];
  }
  double mean = acc / 64;

  uint64_t hash = 0;
  for(int i = 1; i < 65; i++) {
    int y = i / 8;
    int x = i % 8;

    hash <<= 1;
    hash |= mat[y][x] > mean;
  }

  return hash;
}

uint64_t blockHash(const QImage &image) {
  Q_ASSERT(image.width() == 8);
  Q_ASSERT(image.height() == 8);

  quint8 mean[64];

  for(int y = 0; y < 8; y++) {
    const uchar *scanLine = image.constScanLine(y);
    for(int x = 0; x < 8; x++) {
      mean[y * 8 + x] = scanLine[x];
    }
  }

  std::sort(mean, mean + 64);
  int median = (mean[31] + mean[32]) / 2;

  quint64 phash = 0;
  for(int y = 0; y < 8; y++) {
    const uchar *scanLine = image.constScanLine(y);
    for(int x = 0; x < 8; x++) {
      phash <<= 1;
      phash |= scanLine[x] > median;
    }
  }

  return phash;
}

uint64_t differenceHash(const QImage &image) {
  uint64_t hash = 0;

  Q_ASSERT(image.width() == 9);
  Q_ASSERT(image.height() == 8);

  for(int y = 0; y < 8; y++) {
    const uchar *scanLine = image.constScanLine(y);
    for(int x = 0; x < 8; x++) {
      hash <<= 1;
      hash |= scanLine[x] < scanLine[x + 1];
    }
  }

  return hash;
}

QImage autoCrop(const QImage &image, int threshold) {
  int baseColor = image.constScanLine(0)[0];

  int h = image.height();
  int w = image.width();

  int top = 0;
  int left = w;

  int bottom = h;
  int right = 0;

  int y = 0;
  while(y < h && left > 0) {
    const uint8_t *scanLine = image.constScanLine(y);
    int x;
    for(x = 0; x < left; x++) {
      if(std::abs((scanLine[x] - baseColor)) > threshold) {
        break;
      }
    }

    if(x == w) {
      top = y;
    } else if(x < left) {
      // continue
      left = x;
    } else {

    }
    y++;
  }

  y = h - 1;
  while(y >= 0 && right < w) {
    const uint8_t *scanLine = image.constScanLine(y);
    int x;
    for(x = w - 1; x >= right; x--) {
      if(std::abs((scanLine[x] - baseColor)) > threshold) {
        break;
      }
    }

    if(x == -1) {
      bottom = y;
    } else if(x >= right) {
      right = x + 1;
    } else {
      // continue
    }
    y--;
  }

  if(bottom <= top) {
    // Blank image
    left = 0;
    top = 0;
    right = 1;
    bottom = 1;
  }

  if(w != right || h != bottom || left != 0 || top != 0) {
    return image.copy(left, top, right - left, bottom - top);
  }

  return image;
}

bool updateImageMetaData(SQLiteConnection *conn, const QByteArray &imageData, quint64 imageId)
{
  QBuffer buffer((QByteArray *)&imageData);
  QImageReader reader(&buffer);

  QByteArray format = reader.format();
  QSize size = reader.size();

  reader.setBackgroundColor(Qt::darkGray);
  reader.setScaledSize({128, 128});

  QImage image = reader.read();
  if(image.isNull())
    return false;

  auto pixelFormat = image.format();

  image.convertTo(QImage::Format_Grayscale8);
  image = autoCrop(image, 10);
  image = image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  image.convertTo(QImage::Format_Grayscale8);
  uint64_t phash = perceptualHash(image);

  auto ps_update = conn->prepare("UPDATE image SET width = ?1, height = ?2, phash = ?3, format = ?4, filesize = ?5, pixelformat = ?6 WHERE id = ?7");
  ps_update.bind(1, size.width());
  ps_update.bind(2, size.height());
  ps_update.bind(3, phash);
  ps_update.bind(4, QString::fromLatin1(format));
  ps_update.bind(5, imageData.size());
  ps_update.bind(6, (qint64)pixelFormat);
  ps_update.bind(7, imageId);
  ps_update.exec(SRC_LOCATION);

  return true;
}

void FixImageMetaDataTask::run() {
  ImageDao *dao = ImageDao::instance();

  SQLiteConnection conn = dao->connPool()->open();
  QMutexLocker lock(conn.writeLock());

  conn.exec("BEGIN", SRC_LOCATION);
  conn.exec("DELETE FROM thumb40");
  conn.exec("DELETE FROM thumb80");
  conn.exec("DELETE FROM thumb160");
  conn.exec("DELETE FROM thumb320");
  conn.exec("DELETE FROM thumb640");
  conn.exec("DELETE FROM thumb1280");
  {
    auto ps = conn.prepare("SELECT id FROM image ORDER BY id");
    qreal progress = 0.0;
    while(ps.step(SRC_LOCATION)) {
      qint64 id = ps.resultInteger(0);

      RawImageQuery riq(conn, id);
      updateImageMetaData(&conn, riq.data, id);
      emit status->update({}, ++progress);
    }
    emit status->complete();
  }

  conn.exec("COMMIT", SRC_LOCATION);
}

static const uint64_t m1 = 0x5555555555555555; //binary: 0101...
static const uint64_t m2 = 0x3333333333333333; //binary: 00110011..
static const uint64_t m4 = 0x0f0f0f0f0f0f0f0f; //binary:  4 zeros,  4 ones ...

static int popcount64b(uint64_t x) {
  x -= (x >> 1) & m1;             //put count of each 2 bits into those 2 bits
  x = (x & m2) + ((x >> 2) & m2); //put count of each 4 bits into those 4 bits
  x = (x + (x >> 4)) & m4;        //put count of each 8 bits into those 8 bits
  x += x >>  8;  //put count of each 16 bits into their lowest 8 bits
  x += x >> 16;  //put count of each 32 bits into their lowest 8 bits
  x += x >> 32;  //put count of each 64 bits into their lowest 8 bits
  return x & 0x7f;
}


static int hammingDistance(uint64_t a, uint64_t b) {
  return popcount64b(a ^ b);
}


static void findMatchesLinear(const std::vector<uint64_t> &hashes, std::set<uint64_t> &result, int maxd) {
  auto iter_end = hashes.end();
  for(auto i = hashes.begin(); i != iter_end; ++i) {
    bool found_match = false;
    for(auto j = i + 1; j != iter_end; ++j) {
      if(hammingDistance(*i, *j) <= maxd) {
        result.insert(*j);
        found_match = true;
      }
    }
    if(found_match) {
      result.insert(*i);
    }
  }
}

// Each hash belongs to at most one cluster.
using HashToCluster = std::unordered_map<uint64_t, int>;
// Each cluster consists of a number of (unique) hashes.
using ClusterToHashList = std::unordered_map<int, std::vector<uint64_t>>;

static void findClusters(const std::vector<uint64_t> &hashes, HashToCluster &hashToCluster, ClusterToHashList &clusters, int &nextClusterId, int maxd) {
  auto iter_end = hashes.end();
  for(auto i = hashes.begin(); i != iter_end; ++i) {
    uint64_t hash_i = *i;
    for(auto j = i + 1; j != iter_end; ++j) {
      uint64_t hash_j = *j;
      int distance = hammingDistance(hash_i, hash_j);
      if(distance <= maxd) {
        // found a pair
        auto icluster = hashToCluster.find(hash_i);
        auto jcluster = hashToCluster.find(hash_j);

        auto End = hashToCluster.end();

        if(icluster == End && jcluster == End) {
          // both don't belong to a cluster
          int id = nextClusterId++;

          clusters[id].push_back(hash_i);
          clusters[id].push_back(hash_j);

          hashToCluster[hash_i] = id;
          hashToCluster[hash_j] = id;

          qDebug() << "create new cluster" << id << "hashi" << hex << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else if(icluster == End && jcluster != End) {
          // j already belongs to a cluster
          int id = jcluster->second;
          clusters[id].push_back(hash_i);
          hashToCluster[hash_i] = id;

          qDebug() << "merge into j cluster" << id << "hashi" << hex << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else if(icluster != End && jcluster == End) {
          // i already belongs to a cluster
          int id = icluster->second;
          clusters[id].push_back(hash_j);
          hashToCluster[hash_j] = id;

          qDebug() << "merge into i cluster" << id << "hashi" << hex << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else if(icluster->second != jcluster->second) {
          // both belong to different clusters
          int idi = icluster->second;
          int idj = jcluster->second;

          // merge jcluster with icluster
          auto &icluster_data = clusters[idi];
          const auto &jcluster_data = clusters[idj];

          for(auto h : jcluster_data) {
            icluster_data.push_back(h);
            hashToCluster[h] = idi;
          }

          // delete jcluster
          clusters.erase(idj);

          qDebug() << "merge clusters" << idi << "and" << idj << hex << "hashi" << hash_i << "hash_j" << hash_j << "dist" << distance;
        } else {
          // both belong to the same cluster already, nothing to do
        }
      }
    }
  }
}

QList<QObject *> findAllDuplicates(const QList<QObject *> &irefs, int maxDistance)
{
  std::vector<uint64_t> hashList;
  std::unordered_map<uint64_t, std::vector<ImageRef *>> irefLookup;

  ClusterToHashList clusterToHashList;
  HashToCluster hashToCluster;
  int nextClusterId = 0;

  QElapsedTimer timer;
  timer.start();

  for(auto obj : irefs) {
    ImageRef *iref = qobject_cast<ImageRef *>(obj);
    if(iref != nullptr) {
      uint64_t hash = iref->m_phash;

      auto irefIter = irefLookup.find(hash);
      if(irefIter != irefLookup.end()) {
        // hash already exists
        auto cIter = hashToCluster.find(hash);
        if(cIter == hashToCluster.end()) {
          // Hash was seen once before, but a cluster doesn't yet exist.
          int id = nextClusterId++;
          hashToCluster[hash] = id;
          clusterToHashList[id].push_back(hash);
          qDebug() << "create new cluster" << id << "for hash" << hex << hash;
        }

        irefIter->second.push_back(iref);
      } else {
        // new hash
        hashList.push_back(hash);
        irefLookup.emplace(hash, std::vector<ImageRef *>{ iref });
      }
    }
  }

  qDebug() << "pre-existing clusters" << nextClusterId;

  findClusters(hashList, hashToCluster, clusterToHashList, nextClusterId, maxDistance);

  QList<QObject *> output;

  // for each cluster
  for(const auto &p : clusterToHashList) {
    qDebug() << "Cluster Id" << p.first;
    // for each hash
    for(uint64_t h : p.second) {
      // for each iref
      qDebug() << "  Hash" << hex << h;
      for(auto irefptr : irefLookup[h]) {

        qDebug() << "    Iref" << irefptr->m_fileId;
        output.push_back(irefptr);
      }
    }
  }

  qDebug() <<  __FUNCTION__ << "Time:" << timer.elapsed() << "ms Number of results:" << output.size();

  return output;
}
