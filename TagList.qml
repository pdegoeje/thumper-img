import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import thumper

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
    checkable: root.checkable

    backgroundColor: checked ? root.backgroundColorChecked : root.backgroundColor

    checked: model.selected
    tag: model.name
    count: model.count
    onClicked: {
      model.selected = checked
      var tagList = root.model.selectedTags()
      console.log("Tag list", tagList)

      root.clicked(tag, tagList)
    }
  }
}
