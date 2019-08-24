#include "taglist.h"



int QmlTaskListModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;

  return m_tags.size();
}

QVariant QmlTaskListModel::data(const QModelIndex &index, int role) const {
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

bool QmlTaskListModel::setData(const QModelIndex &index, const QVariant &value, int role)
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

QStringList QmlTaskListModel::selectedTags() {
  QStringList result;
  for(const QmlTag &t : m_tags) {
    if(t.selected) {
      result.append(t.name);
    }
  }
  return result;
}

void QmlTaskListModel::update(const QVariantList &tagCount) {
  QMap<QString, int> newTags;

  for(const QVariant &v : tagCount) {
    QVariantList qvl = v.toList();
    Q_ASSERT(qvl.length() == 2);
    newTags.insert(qvl.at(0).toString(), qvl.at(1).toInt());
  }

  auto newIter = newTags.constBegin();
  auto newIterEnd = newTags.constEnd();

  if(tagCount.isEmpty() || m_tags.isEmpty()) {
    // easy case, just reset the entire model.
    beginResetModel();
    m_tags.clear();
    while(newIter != newIterEnd) {
      m_tags.append({ newIter.key(), newIter.value(), false });
      ++newIter;
    }
    endResetModel();
    return;
  }

  // Update data before manipulating the structure.
  // This works around bugs in QML ListView.
  for(int i = 0; i < m_tags.size(); i++) {
    auto newTagIter = newTags.constFind(m_tags[i].name);
    if(newTagIter != newIterEnd) {
      m_tags[i].count = newTagIter.value();
      emit dataChanged(index(i), index(i), { CountRole });
    }
  }

  // Delete non-existing tags
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

  while(i < m_tags.size()) {
    // newTags is a superset of m_tags, so we can't possibly have reached the end before we reach
    // the end of the m_tags list (they can reach their ends at the same time though).
    Q_ASSERT(newIter != newIterEnd);
    // It is impossible for newIter.key() > m_tags[i].name, because we slide
    // over the newIter map until we go in lockstep with m_tags (as in the tags are equal).
    Q_ASSERT(newIter.key() <= m_tags[i].name);

    if(newIter.key() < m_tags[i].name) {
      // new tag, insert it.
      beginInsertRows({}, i, i);
      m_tags.insert(i, { newIter.key(), newIter.value(), false });
      endInsertRows();
    }

    ++i;
    ++newIter;
  }
  // append any leftover new tags.
  while(newIter != newIterEnd) {
    beginInsertRows({}, m_tags.length(), m_tags.length());
    m_tags.append({ newIter.key(), newIter.value(), false });
    endInsertRows();
    ++newIter;
  }
}
