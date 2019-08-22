import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Popup {
  id: root

  anchors.centerIn: Overlay.overlay
  parent: Overlay.overlay
  modal: true

  ColumnLayout {
    CheckBox {
      id: autoTagCheckbox
      checked: autoTagging
      text: "Automatically tag imported images"
      onClicked: autoTagging = checked
    }

    RowLayout {
      Label {
        text: "Export path"
      }

      TextField {
        id: pathPrefixField
        text: pathPrefix
        selectByMouse: true
        onEditingFinished: pathPrefix = text
      }
    }

    RowLayout {
      Label {
        text: "Render size"
      }

      ComboBox {
        Layout.preferredWidth: 150
        model: renderModel
        displayText: "%1px".arg(currentText)
        currentIndex: renderModel.indexOf(renderSize)
        onActivated: {
          renderSize = currentText
        }
      }
    }
  }
}
