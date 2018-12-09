import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.3
import thumper 1.0

ApplicationWindow {
  id: root
  visible: true
  width: 640
  height: 480
  title: qsTr("Thumper")

  property string pathPrefix: pathPrefixField.text
  property int cellSize: 200
  property int actualSize: 500
  property int cellFillMode: Image.PreserveAspectCrop

  ImageProcessor {
    id: processor
  }

  header: ToolBar {
    RowLayout {
      anchors.fill: parent
      Label {
        text: "Prefix"
      }

      TextField {
        id: pathPrefixField
        text: "image"
        selectByMouse: true
      }

      /*
      ToolButton {
        text: qsTr("🖫")
        onClicked: { }
        font.pixelSize: 24
      }*/



      ComboBox {
        model: [100, 200, 300, 400, 500, 600]
        displayText: "Preview Size: %1".arg(currentText)
        currentIndex: 1
        onActivated: {
          cellSize = currentText
        }
      }

      ComboBox {
        model: [100, 200, 300, 400, 500, 600]
        displayText: "Thumb Size: %1".arg(currentText)
        currentIndex: 4
        onActivated: {
          actualSize = currentText
        }
      }

      CheckBox {
        text: "Crop"
        checked: true
        onClicked: {
          cellFillMode = (cellFillMode == Image.PreserveAspectCrop) ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        }
      }

      Label {
        text: "Title"
        elide: Label.ElideRight
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        Layout.fillWidth: true
      }
    }
  }

  Image {
    id: offscreen
    visible: false
    width: actualSize
    height: actualSize
    sourceSize: Qt.size(actualSize, actualSize)
    cache: true
    fillMode: Image.PreserveAspectFit

    onStatusChanged: if(status == Image.Ready) {
      console.log("Image loaded")
      offscreen.grabToImage(function(result) {
        console.log(result.url)
        var gen = processor.nextId(pathPrefix)
        if(gen) {
          var path = pathPrefix + gen + ".png"
          console.log("save to: " + path)
          result.saveToFile(path);
        }
        offscreen.source = ''
      });
    }
  }

  GridView {
    anchors.fill: parent

    id: list

    cellWidth: cellSize
    cellHeight: cellSize

    delegate: Image {
      id: view
      asynchronous: true
      height: cellSize
      width: cellSize
      fillMode: cellFillMode
      cache: true
      mipmap: false
      smooth: true
      source: list.model[index]
      sourceSize.height: cellSize
      sourceSize.width: cellSize

      Rectangle {
        visible: parent.GridView.isCurrentItem
        x: 20
        y: 20
        width: cellSize - 40
        height: cellSize - 40
        color: Qt.rgba(0,0,0,0)
        radius: 10
        border.width: 1
        border.color: 'black'
      }

      TapHandler {
        onTapped: {
          list.currentIndex = index
          list.focus = true
          offscreen.source = view.source
        }
      }
    }

    focus: true
  }

  DropArea {
    anchors.fill: parent
    keys: ["text/uri-list"]

    onEntered: {
      var copy = (drag.supportedActions & Qt.CopyAction) != 0
      drag.accepted = drag.hasUrls && copy
    }

    onDropped: {
      if (drop.hasUrls && drop.proposedAction == Qt.CopyAction) {
        list.model = [].concat(drop.urls)
        drop.acceptProposedAction()
      }
    }
  }

  Shortcut {
    sequence: StandardKey.FullScreen
    onActivated: {
      root.visibility = (root.visibility == Window.FullScreen) ? Window.Windowed : Window.FullScreen
    }
  }
}
