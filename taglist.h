#ifndef TAGLIST_H
#define TAGLIST_H

#include <QObject>
#include <QAbstractListModel>
#include <QHash>
#include <QQmlListProperty>
#include <QQmlEngine>
#include <QSet>

class Tag : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString name MEMBER m_name NOTIFY nameChanged)
  Q_PROPERTY(bool selected MEMBER m_selected NOTIFY selectedChanged)
  Q_PROPERTY(int count MEMBER m_count NOTIFY countChanged)
public:
  Tag(QObject *parent = nullptr) : QObject(parent) { }

  QString m_name;
  bool m_selected = false;
  int m_count = 0;
signals:
  void nameChanged();
  void selectedChanged();
  void countChanged();
};

struct QmlTag {
  QString name;
  int count;
  bool selected;
};

class QmlTaskListModel : public QAbstractListModel {
  Q_OBJECT


  QVector<QmlTag> m_tags;

  // QAbstractItemModel interface
public:
  enum TagRoles {
    NameRole = Qt::UserRole + 1,
    CountRole,
    SelectedRole
  };

  QHash<int, QByteArray> roleNames() const override {
    return {
      { NameRole, "name" },
      { CountRole, "count" },
      { SelectedRole, "selected" },
    };
  }

  int rowCount(const QModelIndex & parent = QModelIndex()) const override {
      if (parent.isValid())
          return 0;

      return m_tags.size();
  }

  QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override {
    if(!hasIndex(index.row(), index.column(), index.parent()))
      return {};

    const QmlTag &item = m_tags.at(index.row());
    switch(role) {
    case NameRole: return item.name;
    case CountRole: return item.count;
    case SelectedRole: return item.selected;
    default: return {};
    }
  }

  bool setData(const QModelIndex &index, const QVariant &value, int role) override
  {
    if (!hasIndex(index.row(), index.column(), index.parent()) || !value.isValid())
        return false;

    QmlTag &item = m_tags[index.row()];
    switch(role) {
    case NameRole: item.name = value.toString(); break;
    case CountRole: item.count = value.toInt(); break;
    case SelectedRole: item.selected = value.toBool(); break;
    default: return false;
    }

    emit dataChanged(index, index, { role } );
    return true ;
  }

  Q_INVOKABLE void update(const QVariantList &tagCount) {
    QMap<QString, int> newTags;

    for(const QVariant &v : tagCount) {
      QVariantList qvl = v.toList();
      newTags.insert(qvl.at(0).toString(), qvl.at(1).toInt());
    }

    QModelIndex parent{};

    // Delete non-existing rows
    for(int i = 0; i < m_tags.size(); ) {
      if(newTags.contains(m_tags.at(i).name)) {
        i++;
      } else {
        beginRemoveRows(parent, i, i);
        m_tags.remove(i);
        endRemoveRows();
      }
    }

    auto i = 0;

    auto newIter = newTags.cbegin();
    auto newIterEnd = newTags.cend();

    while(newIter != newIterEnd && i < m_tags.size()) {
      QmlTag &old = m_tags[i];
      int cmp = newIter.key().compare(old.name);
      if(cmp < 0) {
        beginInsertRows(parent, i, i);
        m_tags.insert(i, { newIter.key(), newIter.value(), false });
        endInsertRows();

        ++i;
        ++newIter;
      } else if(cmp > 0) {
        ++i;
      } else {
        m_tags[i].count = newIter.value();
        QModelIndex curIndex = index(i);
        emit dataChanged(curIndex, curIndex, { CountRole });
        ++i;
        ++newIter;
      }
    }
    while(newIter != newIterEnd) {
      beginInsertRows(parent, i, i);
      m_tags.append({ newIter.key(), newIter.value(), false });
      endInsertRows();
      ++newIter;
    }
  }
};

class TagListModel : public QObject {
  Q_OBJECT
  Q_PROPERTY(QQmlListProperty<Tag> tags READ tags NOTIFY tagsChanged)
  Q_PROPERTY(QAbstractListModel *listModel READ listModel)
  QList<Tag *> m_tags;

  QAbstractListModel *m_listModel;
public:
  TagListModel(QObject *parent = nullptr) : QObject(parent) {

  }

  QQmlListProperty<Tag> tags() {
    return QQmlListProperty<Tag>((QObject *)this, m_tags);
  }

  QAbstractListModel *listModel() const { return m_listModel; }

  Q_INVOKABLE QVariantList makeTagList(const QVariantList &tagCount) {
    QVariantList result;
    for(const QVariant &v : tagCount) {
      QVariantList qvl = v.toList();

      Tag *tag = new Tag();
      //QQmlEngine::setObjectOwnership(tag, QQmlEngine::JavaScriptOwnership);

      tag->m_name = qvl.at(0).toString();
      tag->m_count = qvl.at(1).toInt();

      result.append(QVariant::fromValue(tag));
    }
    return result;
  }

  Q_INVOKABLE void update(const QVariantList &tagCount) {
    for(Tag *t : m_tags) {
      t->deleteLater();
    }

    m_tags.clear();
    for(const QVariant &v : tagCount) {
      QVariantList qvl = v.toList();

      Tag *tag = new Tag(this);

      tag->m_name = qvl.at(0).toString();
      tag->m_count = qvl.at(1).toInt();

      m_tags.append(tag);
    }

    emit tagsChanged();
  }
signals:
  void tagsChanged();
};

#endif // TAGLIST_H
