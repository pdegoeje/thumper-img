import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import thumper 1.0

ApplicationWindow {
  id: root
  visible: true
  width: 640
  height: 480
  title: "Thumper 1.1.0"

  property string pathPrefix: pathPrefixField.text
  property int imagesPerRow: 6
  property int imageWidth: (list.width - spacing) / imagesPerRow - spacing
  property int imageHeight: imageWidth * aspectRatio
  property int spacing: 8
  property int actualSize: 531
  property int cellFillMode: Image.PreserveAspectCrop
  property var sizeModel: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 20]
  property var renderModel: [ 160, 240, 320, 480, 531, 640 ]
  property real aspectRatio: 1.5
  property var aspectRatioModel: [0.5, 0.67, 1.0, 1.5, 2.0]

  color: '#333'

  ListModel {
    id: imageList
  }

  property var selectionModel: []
  property var selectionTagCount: []
  property var allTagsCount: []

  property var searchTagsModel: []

  function rebuildSelectionModel() {
    var newModel = []
    for(var i = 0; i < imageList.count; i++) {
      if(imageList.get(i).selected)
        newModel.push(imageList.get(i).fileId)
    }
    selectionModel = newModel
    selectionTagCount = ImageDao.tagsByMultipleIds(selectionModel)
    allTagsCount = ImageDao.allTagsCount()
  }

  function displayImage(fileId) {
    imageList.append({ 'url': 'image://thumper/' + fileId,
                       'fileId': fileId,
                       'selected' : false
                     })
  }

  ImageProcessor {
    id: processor
    onImageReady: {
      var fileName = urlFileName(url)
      console.log("File saved", url)
      var regex = /^([a-z-]+)[0-9]*\.(\w+)$/
      if(regex.test(fileName)) {
        var result = fileName.match(regex)

        var basename = result[1]
        var extension = result[2]

        var word = /[a-z]+/g

        var foundTags = basename.match(word)

        console.log("Tags found", foundTags)
        for(var i in foundTags) {
          ImageDao.addTag(fileId, foundTags[i])
        }
      }

      displayImage(fileId)
    }
  }

  onSearchTagsModelChanged: {
    console.log("searchTagsModel changed", searchTagsModel, searchTagsModel.length)
    var ids
    if(searchTagsModel.length > 0) {
      console.log("Search")
      ids = ImageDao.idsByTags(searchTagsModel)
    } else {
      console.log("All Ids")
      ids = ImageDao.allIds()
    }
    imageList.clear()
    for(var i in ids) {
      displayImage(ids[i])
    }
  }

  header: ToolBar {
    id: toolbar
    ColumnLayout {
      RowLayout {

        Label {
          text: "Path"
        }

        TextField {
          id: pathPrefixField
          text: "./"
          selectByMouse: true
        }

        ComboBox {
          model: sizeModel
          displayText: "Columns: %1".arg(currentText)
          currentIndex: sizeModel.indexOf(imagesPerRow)
          onActivated: {
            imagesPerRow = currentText
          }
        }

        ComboBox {
          model: renderModel
          displayText: "Render: %1px".arg(currentText)
          currentIndex: renderModel.indexOf(actualSize)
          onActivated: {
            actualSize = currentText
          }
        }

        ComboBox {
          model: aspectRatioModel
          displayText: "Aspect: %1".arg(currentText)
          currentIndex: aspectRatioModel.indexOf(aspectRatio)
          onActivated: {
            aspectRatio = currentText
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

      RowLayout {
        Label {
          text: "Search"
        }

        TextField {
          onAccepted: {
            var input = text.trim()
            if(input !== '') {
              searchTagsModel = input.split(" ")
            } else {
              searchTagsModel = []
            }

            console.log(searchTagsModel)
          }
        }
      }
    }
  }

  footer: Rectangle {
    visible: toolbar.visible
    height: myFlow.implicitHeight

    color: 'transparent'

    Flow {
      padding: 4

      id: myFlow
      anchors.left: parent.left
      anchors.right: parent.right
      spacing: 4
      Label {
        padding: 4
        background: Rectangle {
          color: '#cc000000'
        }
        color: 'white'

        text: "Selected %1 images".arg(selectionModel.length)
      }

      Repeater {
        model: selectionTagCount
        Tag {
          property string tag: modelData[0]
          property int count: modelData[1]
          backgroundColor: Qt.tint('green', Qt.rgba(0, 0, 0, (1 - count / selectionModel.length) * 0.5))
          text: tag + (count > 1 ? " x" + count : "")

          onClicked: {
            if(count == selectionModel.length) {
              ImageDao.removeTagFromMultipleIds(selectionModel, tag)
              rebuildSelectionModel()
            } else {
              ImageDao.addTagToMultipleIds(selectionModel, tag)
              rebuildSelectionModel()
            }
          }
        }
      }

      Repeater {
        model: allTagsCount
        Tag {
          property string tag: modelData[0]
          property int count: modelData[1]

          active: false
          text: tag + (count > 1 ? " x" + count : "")

          onClicked: {
            ImageDao.addTagToMultipleIds(selectionModel, tag)
            rebuildSelectionModel()
          }
        }
      }

      Tag {
        text: 'Add Tag'
        backgroundColor: 'blue'
        onClicked: {
          addTagPopup.open()
        }
      }
    }
  }

  Popup {
    id: addTagPopup
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    focus: true

    RowLayout {
      Label {
        text: "Add tag"
      }

      TextField {
        focus: true
        id: tagField
        selectByMouse: true
        onAccepted: {
          ImageDao.addTagToMultipleIds(selectionModel, text)
          rebuildSelectionModel()
          addTagPopup.close()
        }
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

    property string fileId

    onStatusChanged: if(status == Image.Ready) {
      offscreen.grabToImage(function(result) {
        console.log(result.url)
        var path = pathPrefix + fileId + ".jpg"
        console.log("Saved to: " + path)
        processor.setClipBoard(fileId)
        result.saveToFile(path);

        offscreen.fileId = ''
        offscreen.source = ''
      });
    }
  }

  Component {
    id: highlight
    Rectangle {
      width: list.cellWidth + spacing
      height: list.cellHeight + spacing
      color: '#555' //'green'//'transparent'
      border.color: "lightsteelblue"
      opacity: list.activeFocus ? 1 : 0.3
      border.width: spacing
      x: list.currentItem ? list.currentItem.x - list.pad : 0
      y: list.currentItem ? list.currentItem.y - list.pad : 0
    }
  }
  Control {
    focusPolicy: Qt.StrongFocus
    anchors.fill: parent

    onActiveFocusChanged: {
      if(focus) {
        list.forceActiveFocus()
      }
    }

    contentItem: GridView {
      anchors.fill: parent

      id: list

      focus: true

      model: imageList
      property int pad: spacing / 2

      topMargin: pad
      bottomMargin: pad
      leftMargin: pad
      rightMargin: pad

      cellWidth: imageWidth + spacing
      cellHeight: imageHeight + spacing

      highlight: highlight
      highlightFollowsCurrentItem: false
      //highlightMoveDuration: 0

      pixelAligned: true
      interactive: true

      TapHandler {
        onTapped: {
          list.forceActiveFocus()
        }
      }

      delegate: Item {
        width: list.cellWidth
        height: list.cellHeight

        Image {
          x: list.pad
          y: list.pad

          id: view
          asynchronous: true
          height: imageHeight
          width: imageWidth
          fillMode: cellFillMode
          cache: true
          mipmap: false
          smooth: true
          source: url
          sourceSize.height: height
          sourceSize.width: width

          opacity: (selectionModel.length > 0 && !selected) ? 0.4 : 1

          CheckBox {
            focusPolicy: Qt.NoFocus
            checked: selected
            onClicked: {
              if(selected != checked) {
                selected = checked
                rebuildSelectionModel()
              }
            }
          }

          TapHandler {
            onTapped: {
              list.currentIndex = index
              offscreen.fileId = fileId
              offscreen.source = view.source
            }
          }
        }
      }
    }
  }

  Loader {
    id: lightboxLoader
    active: false

    sourceComponent: Popup {
      id: lightbox

      property string currentFileId: imageList.get(list.currentIndex).fileId

      parent: Overlay.overlay
      anchors.centerIn: Overlay.overlay
      dim: true
      visible: true

      padding: 0

      onClosed: lightboxLoader.active = false

      Overlay.modeless: Rectangle {
        color:"#CC000000"
      }

      background: Item {
        MouseArea {
          anchors.fill: parent
          onClicked: lightbox.close()
        }
      }

      Item {
        implicitWidth: content.paintedWidth
        implicitHeight: content.paintedHeight

        Image {
          id: content

          asynchronous: true

          anchors.centerIn: parent

          sourceSize.width: root.width - 40
          sourceSize.height: root.height - 40

          smooth: true
          width: sourceSize.width
          height: sourceSize.height
          fillMode: Image.PreserveAspectFit
          source: imageList.get(list.currentIndex).url
        }
      }
    }
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
    sequence: "Ctrl+D"
    onActivated: {
      console.log("Deselect")
      for(var i = 0; i < imageList.count; i++) {
        //imageList.get(i).selected = false
        imageList.setProperty(i, 'selected', false)
      }
      rebuildSelectionModel()
    }
  }

  Shortcut {
    sequence: "Ctrl+A"
    onActivated: {
      console.log("Select all")
      for(var i = 0; i < imageList.count; i++) {
        imageList.setProperty(i, 'selected', true)
      }
      rebuildSelectionModel()
    }
  }

  Shortcut {
    sequence: "Ctrl+Space"
    onActivated: {
      lightboxLoader.active = !lightboxLoader.active
    }
  }
}
