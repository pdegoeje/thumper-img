import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import thumper

Popup {
  id: root

  property var tagList: []
  property var initialSelection: []

  function edit(initialSelection, allTagCount) {
    root.initialSelection = initialSelection
    root.tagList = allTagCount
    root.open()
  }

  signal editComplete(var selectedTags)

  parent: Overlay.overlay
  anchors.centerIn: Overlay.overlay
  modal: true
  focus: true

  ColumnLayout {
    Label {
      Layout.alignment: Qt.AlignCenter
      text: "Select images matching all selected tags."
    }
    Flow {
      Layout.fillWidth: true
      Layout.maximumWidth: 640
      spacing: 8
      Repeater {
        id: tagRepeater
        model: tagList
        Tag {
          tag: modelData[0]
          count: modelData[1]
          checked: initialSelection.indexOf(tag) != -1
          checkable: true
          backgroundColor: checked ? 'green' : 'grey'
          
          text: tag + (count > 1 ? " (%1)".arg(count) : "")
        }
      }
    }
    Button {
      focus: true
      Layout.alignment: Qt.AlignCenter
      text: "OK"
      onClicked: {
        console.log("done")
        var selectedTags = []
        for(var i = 0; i < tagList.length; i++) {
          if(tagRepeater.itemAt(i).checked) {
            selectedTags.push(tagList[i][0])
          }
        }

        root.close()
        editComplete(selectedTags)
      }
    }
  }
}
