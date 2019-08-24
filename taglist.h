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

  int rowCount(const QModelIndex & parent = QModelIndex()) const override;
  QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role) override;
  Q_INVOKABLE QStringList selectedTags();
  Q_INVOKABLE void update(const QVariantList &tagCount);
};

#endif // TAGLIST_H
