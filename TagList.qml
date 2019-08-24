import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.5
import QtQuick.Controls.Material 2.12
import thumper 1.0

ListView {
  id: root
  orientation: ListView.Horizontal
  signal clicked(string tag, var checkedTags)

  property color backgroundColor: '#7b1fa2'
  property color backgroundColorChecked: '#388e3c'
  property bool checkable: false

  pixelAligned: true
  clip: true

  spacing: 4

  delegate: Tag {
    property TagModel tagModel: modelData

    id: theTag
    checkable: root.checkable

    backgroundColor: checked ? root.backgroundColorChecked : root.backgroundColor

    checked: tagModel.selected
    tag: tagModel.name
    count: tagModel.count
    onClicked: {
      tagModel.selected = checked
      var tagList = root.model

      console.log(tagList)

      var checkedTags = []
      for(var i = 0; i < tagList.length; i++) {
        if(tagList[i].selected) {
          checkedTags.push(tagList[i].name)
        }
      }

      root.clicked(tag, checkedTags)
    }
  }
}
