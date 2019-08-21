import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.12
import QtQml 2.12
import thumper 1.0

ApplicationWindow {
  id: root
  visible: true
  width: 1280
  height: 720
  title: "Thumper 1.3.0"

  property string pathPrefix: pathPrefixField.text
  property int imagesPerRow: 6
  property int imageWidth: (list.width - spacing) / imagesPerRow - spacing
  property int imageHeight: imageWidth * aspectRatio
  property int spacing: 8
  property int actualSize: 531
  property int cellFillMode: Image.PreserveAspectCrop
  property var sizeModel: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 24, 32, 48, 64]
  property var renderModel: [ 160, 240, 320, 480, 531, 640 ]
  property real aspectRatio: 1.5
  property var aspectRatioModel: [0.5, 0.67, 1.0, 1.5, 2.0]
  property alias autoTagging: autoTagCheckbox.checked

  property var viewIdToIndexMap: ({})
  ListModel {
    id: viewModel
  }
  property var viewModelSimpleList: []
  property var allSimpleList: ImageDao.all()

  property var selectionModel: []


  property var selectionTagCount: []
  property var viewTagCount: []

  property var searchTagsModel: []
  property var allTagCount: []

  onSelectionModelChanged: rebuildTagModels()

  function rebuildTagModels() {
    selectionTagCount = ImageDao.tagCount(selectionModel)
    viewTagCount = ImageDao.tagCount(viewModelSimpleList)
    allTagCount = ImageDao.tagCount(allSimpleList)
  }

  function rebuildSelectionModel() {
    selectionModel = viewModelSimpleList.filter(function(ref) { return ref.selected })
  }

  function viewClear() {
    viewIdToIndexMap = ({})
    viewModel.clear()
    viewModelSimpleList = []
  }

  function viewAppendMultiple(refList) {
    var modelRefList = []
    for(var i = 0; i < refList.length; i++) {
      var ref = refList[i]
      viewIdToIndexMap[ref.fileId] = viewModelSimpleList.length
      viewModelSimpleList.push(ref)
      modelRefList.push({ 'ref' : ref })
    }

    viewModel.append(modelRefList)
  }

  ImageProcessor {
    id: processor
    onImageReady: {
      var ref = ImageDao.findHash(hash)
      var fileName = urlFileName(url)
      var regex = /^([a-zA-Z-_]+)[0-9]*\.(\w+)$/
      if(autoTagging && regex.test(fileName)) {
        var result = fileName.match(regex)

        var basename = result[1]
        var extension = result[2]

        var word = /[a-zA-Z]+/g

        var foundTags = basename.match(word)

        console.log("Tags found", foundTags)
        ImageDao.transactionStart()
        for(var i in foundTags) {
          ImageDao.addTag(ref, foundTags[i])
        }
        ImageDao.transactionEnd()
      }

      allSimpleList.push(ref)
      viewAppendMultiple([ref])
    }
  }

  onSearchTagsModelChanged: {
    var refList
    if(searchTagsModel.length > 0) {
      refList = ImageDao.search(allSimpleList, searchTagsModel)
    } else {
      refList = allSimpleList
    }   

    // force the view to reset, this is a performance optimization
    list.model = undefined

    viewClear()
    viewAppendMultiple(refList)
    rebuildSelectionModel()

    list.model = viewModel
  }

  header: ToolBar {
    id: toolbar
    leftPadding: 10
    rightPadding: 10
    RowLayout {
      anchors.left: parent.left
      anchors.right: parent.right

      TextField {
        id: searchField
        Layout.preferredWidth: 200
        placeholderText: "Search"
        selectByMouse: true
        onAccepted: {
          var input = text.trim()
          if(input !== '') {
            searchTagsModel = input.split(" ")
          } else {
            searchTagsModel = []
          }
        }
      }

      TagList {
        model: allTagCount
        checkable: true
        onClicked: {
          searchTagsModel = checkedTags
          searchField.text = searchTagsModel.join(' ')
        }
      }

      ComboBox {
        Layout.preferredWidth: 150
        model: sizeModel
        displayText: "%1 Columns".arg(currentText)
        currentIndex: sizeModel.indexOf(imagesPerRow)
        onActivated: {
          imagesPerRow = currentText
        }
      }

      ComboBox {
        Layout.preferredWidth: 150
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
      CheckBox {
        id: autoTagCheckbox
        text: "Autotag"
      }

      Label {
        text: "Export path"
      }

      TextField {
        id: pathPrefixField
        text: "./"
        selectByMouse: true
      }
      ComboBox {
        Layout.preferredWidth: 150
        model: renderModel
        displayText: "Export %1px".arg(currentText)
        currentIndex: renderModel.indexOf(actualSize)
        onActivated: {
          actualSize = currentText
        }
      }
      Item { Layout.fillWidth: true }
    }
  }

  Popup {
    id: addTagPopup
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    focus: true

    onAboutToShow: tagField.text = ''

    RowLayout {
      Label {
        text: "Add tag"
      }

      TextField {
        focus: true
        id: tagField
        selectByMouse: true
        onAccepted: {
          var tag = text.trim()
          if(tag !== '') {
            ImageDao.transactionStart()
            selectionModel.forEach(function(ref) { ImageDao.addTag(ref, tag) })
            rebuildTagModels()
            ImageDao.transactionEnd()
          }
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

  GridView {
    activeFocusOnTab: true
    anchors.fill: parent

    id: list

    focus: true

    model: viewModel
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

    pixelAligned: false
    interactive: true

    property bool isScrolling: false

    onFlickStarted: isScrolling = true
    onFlickEnded: isScrolling = false

    ScrollBar.vertical: ScrollBar {
      onPressedChanged: list.isScrolling = pressed
    }

    delegate: Item {
      id: delegateItem
      width: list.cellWidth
      height: list.cellHeight

      property ImageRef image: ref

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
        source: "image://thumper/" + delegateItem.image.fileId
        sourceSize.height: height
        sourceSize.width: width

        opacity: (view.status == Image.Ready) ? ((selectionModel.length > 0 && !delegateItem.image.selected) ? 0.4 : 1) : 0

        Behavior on opacity {
          NumberAnimation { duration: 100 }
        }

        // don't even bother loading if the image isn't ready for display
        Loader {
          id: perItemUILoader

          active: false
          property bool shouldProbablyLoad: view.status == Image.Ready && toolbar.visible && !list.isScrolling
          property bool beforeLoad: true

          onShouldProbablyLoadChanged: {
            active = true
            beforeLoad = false
          }

          sourceComponent: CheckBox {
            opacity: (list.isScrolling || perItemUILoader.beforeLoad) ? 0 : 1

            focusPolicy: Qt.NoFocus
            checked: delegateItem.image.selected
            onClicked: {
              if(delegateItem.image.selected != checked) {
                delegateItem.image.selected = checked
                rebuildSelectionModel()
              }
            }
            Behavior on opacity {
              OpacityAnimator { duration: 150 }
            }
          }
        }

        TapHandler {
          acceptedModifiers: Qt.AltModifier
          onTapped: {
            offscreen.fileId = fileId
            offscreen.source = view.source
          }
        }

        TapHandler {
          acceptedModifiers: Qt.NoModifier
          onTapped: {
            list.forceActiveFocus()
            list.currentIndex = index
          }

          onDoubleTapped: {
            lightboxLoader.active = true
          }
        }

        TapHandler {
          acceptedModifiers: Qt.ControlModifier
          onTapped: {
            delegateItem.image.selected = !delegateItem.image.selected
            rebuildSelectionModel()
          }
        }
      }
    }
  }

  TagSelection {
    id: tagSelection

    onEditComplete: {
      var result = ImageDao.search(viewModelSimpleList, selectedTags)
      if(result.length > 0) {
        viewModelSimpleList.forEach(function(ref) { ref.selected = false })
        result.forEach(function(ref) { ref.selected = true })
        rebuildSelectionModel()

        var index = viewIdToIndexMap[result[0].fileId]
        list.positionViewAtIndex(index, GridView.Center)
      }
    }
  }

  footer: ToolBar {
    visible: toolbar.visible
    leftPadding: 10
    rightPadding: 10
    RowLayout {
      anchors.left: parent.left
      anchors.right: parent.right

      id: myFlow
      Label {
        text: "Selected %1/%2".arg(selectionModel.length).arg(viewModel.count)
      }

      TagList {
        model: selectionTagCount
        //backgroundColor: Qt.tint('green', Qt.rgba(0, 0, 0, (1 - count / selectionModel.length) * 0.5))

        backgroundColor: 'green'
        onClicked: {
          ImageDao.transactionStart()
          selectionModel.forEach(function(ref) { ImageDao.removeTag(ref, tag) })
          rebuildTagModels()
          ImageDao.transactionEnd()
        }

      }

      Item {
        Layout.fillWidth: true
      }

      Button {
        text: "Select"

        onClicked: {
          var mysel = selectionTagCount.map(function(x) { return x[0]})
          tagSelection.edit(mysel, viewTagCount)
        }
      }

      Label {
        text: "Add"
      }

      Tag {
        text: 'new'
        backgroundColor: Material.accentColor
        onClicked: {
          addTagPopup.open()
        }
      }

      TagList {
        model: allTagCount
        onClicked: {
          ImageDao.transactionStart()
          selectionModel.forEach(function(ref) { ImageDao.addTag(ref, tag) })
          rebuildSelectionModel()
          ImageDao.transactionEnd()
        }
      }
    }
  }

  Loader {
    id: lightboxLoader
    active: false

    sourceComponent: Popup {
      id: lightbox

      property ImageRef image: viewModel.get(list.currentIndex).ref

      parent: Overlay.overlay
      anchors.centerIn: Overlay.overlay
      dim: true
      visible: true

      padding: 0

      onClosed: lightboxLoader.active = false

      Overlay.modeless: Rectangle {
        color: '#99101010'
        Behavior on opacity { NumberAnimation { duration: 150 } }
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

          opacity: (status == Image.Ready) ? 1 : 0

          smooth: true
          width: sourceSize.width
          height: sourceSize.height
          fillMode: Image.PreserveAspectFit
          source: "image://thumper/" + lightbox.image.fileId

          Behavior on opacity {
            NumberAnimation { duration: 100 }
          }
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
      for(var i = 0; i < viewModel.count; i++) {
        viewModel.get(i).ref.selected = false
      }
      rebuildSelectionModel()
    }
  }

  Shortcut {
    sequence: "Ctrl+A"
    onActivated: {
      console.log("Select all")
      for(var i = 0; i < viewModel.count; i++) {
        viewModel.get(i).ref.selected = true
      }
      rebuildSelectionModel()
    }
  }

  Shortcut {
    sequence: "Ctrl+I"
    onActivated: {
      console.log("Invert Selection")
      for(var i = 0; i < viewModel.count; i++) {
        var ref = viewModel.get(i).ref
        ref.selected = !ref.selected
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
