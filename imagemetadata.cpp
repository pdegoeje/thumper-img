#include "imagemetadata.h"
#include "imagedao.h"
#include "sqlitehelper.h"

#include "dct/fast-dct-lee.h"

#include <QImageReader>
#include <QBuffer>

uint64_t FixImageMetaDataTask::perceptualHash(const QImage &image) {
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

uint64_t FixImageMetaDataTask::blockHash(const QImage &image) {
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

uint64_t FixImageMetaDataTask::differenceHash(const QImage &image) {
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

QImage FixImageMetaDataTask::autoCrop(const QImage &image, int threshold) {
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

void FixImageMetaDataTask::run() {
  ImageDao *dao = ImageDao::instance();
  SQLiteConnection *conn = dao->connPool()->open();

  conn->exec("BEGIN TRANSACTION", __FUNCTION__);
  {
    SQLitePreparedStatement ps_update(conn, "UPDATE image SET width = ?1, height = ?2, phash = ?3 WHERE id = ?4");
    SQLitePreparedStatement ps(conn, "SELECT id FROM image ORDER BY id");
    qreal progress = 0.0;
    while(ps.step(__FUNCTION__)) {
      qint64 id = ps.resultInteger(0);

      ImageDao::ImageDataContext idc;
      dao->imageDataAcquire(idc, id);
      QBuffer buffer(&idc.data);
      QImageReader reader(&buffer);

      QSize size = reader.size();

      reader.setBackgroundColor(Qt::darkGray);
      reader.setScaledSize({128, 128});
      //reader.setScaledSize({9, 8});

      QImage image = reader.read();
      if(!image.isNull()) {
        image.convertTo(QImage::Format_Grayscale8);
        //image.save(QStringLiteral("%1_crop_input.png").arg(id));
        image = autoCrop(image, 10);
        //image.save(QStringLiteral("%1_crop_output.png").arg(id));
        image = image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        image.convertTo(QImage::Format_Grayscale8);
        //image.save(QStringLiteral("%1_phash_input.png").arg(id));
        uint64_t phash = perceptualHash(image);
        //qDebug() << "id" << id << "phash" << phash;

        ps_update.bind(1, size.width());
        ps_update.bind(2, size.height());
        ps_update.bind(3, phash);
        ps_update.bind(4, id);
        ps_update.exec(__FUNCTION__);
      }

      dao->imageDataRelease(idc);

      emit status->update({}, ++progress);
    }
    emit status->complete();
  }

  conn->exec("END TRANSACTION", __FUNCTION__);

  dao->connPool()->close(conn);
}
