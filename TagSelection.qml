import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.12
import thumper 1.0

Popup {
  id: root

  property var tagList: []
  property var initialSelection: []

  function edit(initialSelection) {
    root.initialSelection = initialSelection
    root.tagList = ImageDao.allTagsCount()
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
          property string tag: modelData[0]
          property int count: modelData[1]
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
