#ifndef TAGLIST_H
#define TAGLIST_H

#include <QObject>
#include <QAbstractListModel>
#include <QHash>
#include <QQmlListProperty>
#include <QQmlEngine>
#include <QSet>
#include <QDebug>
#include <QTimer>

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

  Q_INVOKABLE QStringList selectedTags() {
    QStringList result;
    for(const QmlTag &t : m_tags) {
      if(t.selected) {
        result.append(t.name);
      }
    }
    return result;
  }

  Q_INVOKABLE void update(const QVariantList &tagCount) {
    QMap<QString, int> newTags;

    for(const QVariant &v : tagCount) {
      QVariantList qvl = v.toList();
      Q_ASSERT(qvl.length() == 2);
      newTags.insert(qvl.at(0).toString(), qvl.at(1).toInt());
    }

    auto newIter = newTags.constBegin();
    auto newIterEnd = newTags.constEnd();

    // Do this explicitly before manipulating the model.
    // This works around bugs in QML ListView.
    for(int i = 0; i < m_tags.size(); i++) {
      auto newTagIter = newTags.constFind(m_tags[i].name);
      if(newTagIter != newIterEnd) {
        m_tags[i].count = newTagIter.value();
        emit dataChanged(index(i), index(i), { CountRole });
      }
    }

    // Delete non-existing rows
    for(int i = 0; i < m_tags.size(); ) {
      if(newTags.contains(m_tags[i].name)) {
        i++;
      } else {
        beginRemoveRows({}, i, i);
        m_tags.remove(i);
        endRemoveRows();
      }
    }

    int i = 0;

    // newTags is now a superset of the existing m_tags
    while(newIter != newIterEnd && i < m_tags.size()) {
      QmlTag &old = m_tags[i];
      int cmp = newIter.key().compare(old.name);
      if(cmp < 0) {
        beginInsertRows({}, i, i);
        m_tags.insert(i, { newIter.key(), newIter.value(), false });
        endInsertRows();

        ++i;
        ++newIter;
      } else if(cmp > 0) {
        // this can never happen, because all m_tags are in the newTags map.
        ++i;
      } else {
        ++i;
        ++newIter;
      }
    }
    while(newIter != newIterEnd) {
      beginInsertRows({}, m_tags.length(), m_tags.length());
      m_tags.append({ newIter.key(), newIter.value(), false });
      endInsertRows();
      ++newIter;
    }
  }
};

#endif // TAGLIST_H
