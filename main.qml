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
  title: "Thumper 1.0.1"

  property string pathPrefix: pathPrefixField.text
  property int cellSize: 240
  property int actualSize: 531
  property int cellFillMode: Image.PreserveAspectCrop
  property var sizeModel:  [160, 240, 320, 480, 531, 640]

  color: '#333'

  ListModel {
    id: imageList
  }

  ImageProcessor {
    id: processor

    onImageReady: {
      console.log("Image ready: " + fileId)
      imageList.append({ url: 'image://thumper/' + fileId })
    }

    Component.onCompleted: {
      loadExisting()
    }
  }

  header: ToolBar {
    id: toolbar
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

      ComboBox {
        model: sizeModel
        displayText: "Preview: %1px".arg(currentText)
        currentIndex: 1
        onActivated: {
          cellSize = currentText
        }
      }

      ComboBox {
        model: sizeModel
        displayText: "Render: %1px".arg(currentText)
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

      Item {
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
          var path = pathPrefix + gen + ".jpg"
          console.log("save to: " + path)
          result.saveToFile(path);
        }
        offscreen.source = ''
      });
    }
  }

  Component {
    id: highlight
    Rectangle {
      width: list.cellWidth + list.spacing
      height: list.cellHeight + list.spacing
      color: 'transparent'
      border.color: "lightsteelblue"
      border.width: list.spacing
      x: list.currentItem.x - list.pad
      y: list.currentItem.y - list.pad
    }
  }

  GridView {
    anchors.fill: parent

    id: list

    model: imageList
    property int pad: spacing / 2
    property int spacing: 10

    topMargin: pad
    bottomMargin: pad
    leftMargin: pad
    rightMargin: pad

    cellWidth: cellSize + spacing
    cellHeight: cellSize + spacing

    highlight: highlight
    highlightFollowsCurrentItem: false
    //highlightMoveDuration: 0

    pixelAligned: true
    interactive: true

    delegate: Item {
      width: list.cellWidth
      height: list.cellHeight

      Image {
        x: list.pad
        y: list.pad

        id: view
        asynchronous: true
        height: cellSize
        width: cellSize
        fillMode: cellFillMode
        cache: false
        mipmap: false
        smooth: true
        source: url
        sourceSize.height: cellSize
        sourceSize.width: cellSize

        TapHandler {
          onTapped: {
            list.currentIndex = index
            list.focus = true
            offscreen.source = view.source
          }
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
        drop.acceptProposedAction()

        drop.urls.forEach(function(x) {
          processor.download(x)
        });
      }
    }
  }

  Shortcut {
    sequence: StandardKey.FullScreen
    onActivated: {
      root.visibility = (root.visibility == Window.FullScreen) ? Window.Windowed : Window.FullScreen
    }
  }

  Shortcut {
    sequence: "Ctrl+H"
    onActivated: {
      toolbar.visible = !toolbar.visible
    }
  }

  Shortcut {
    sequence: "Ctrl+T"
    onActivated: {
      imageList.clear()
    }
  }
}
