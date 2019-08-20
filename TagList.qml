import QtQuick 2.12
import QtQuick.Layouts 1.12

RowLayout {
  id: root
  property alias model: repeater.model

  signal clicked(string tag, var checkedTags)

  property color backgroundColor: '#7b1fa2'
  property color backgroundColorChecked: '#388e3c'
  property bool checkable: false

  Repeater {
    id: repeater

    Tag {
      id: theTag
      checkable: root.checkable

      backgroundColor: checked ? root.backgroundColorChecked : root.backgroundColor

      tag: modelData[0]
      count: modelData[1]
      onClicked: {
        var checkedTags = []
        for(var i = 0; i < repeater.count; i++) {
          var item = repeater.itemAt(i)
          if(item.checked) {
            checkedTags.push(item.tag)
          }
        }

        root.clicked(tag, checkedTags)
      }
    }
  }
}
