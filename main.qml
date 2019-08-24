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
  title: "Thumper 1.5.0"

  property string pathPrefix: "./"
  property int imagesPerRow: 6
  property var imagesPerRowModel: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 24, 32, 48, 64]
  property int imageWidth: (list.width - spacing) / imagesPerRow - spacing
  property int imageHeight: imageWidth * aspectRatio
  property int spacing: 8
  property int renderSize: 531
  property int cellFillMode: Image.PreserveAspectCrop
  property var renderModel: [ 160, 240, 320, 480, 531, 640 ]
  property real aspectRatio: 1
  property var aspectRatioModel: [0.5, 0.67, 1.0, 1.5, 2.0]
  property bool autoTagging: false

  property bool gridShowImageIds: true
  property bool gridShowSelectors: true

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

  QmlTaskListModel { id: selectionTagModelList }
  QmlTaskListModel { id: viewTagModelList }
  QmlTaskListModel {
    id: allTagModelList
    Component.onCompleted: rebuildAllTagModel()
  }

  onSelectionModelChanged: rebuildTagModels()

  function rebuildTagModels() {
    selectionTagCount = ImageDao.tagCount(selectionModel)
    viewTagCount = ImageDao.tagCount(viewModelSimpleList)

    selectionTagModelList.update(selectionTagCount)
    viewTagModelList.update(viewTagCount)
  }

  function rebuildAllTagModel() {
    allTagModelList.update(ImageDao.tagCount(allSimpleList))
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

  property var actionHistory: []

  function actionAddTag(refList, tag, record = true) {
    var actionList = []

    ImageDao.lockWrite();
    actionList = ImageDao.addTagMultiple(refList, tag)
    ImageDao.unlockWrite();
    rebuildTagModels()
    rebuildAllTagModel()

    console.log("Added tag", tag, "to", actionList.length, "image(s)")

    if(record) {
      actionHistory.push(actionRemoveTag.bind(null, refList, tag));
    }
  }


  function actionRemoveTag(refList, tag, record = true) {
    var actionList = []

    ImageDao.lockWrite();
    actionList = ImageDao.removeTagMultiple(refList, tag);
    ImageDao.unlockWrite();
    rebuildTagModels()
    rebuildAllTagModel()

    console.log("Removed tag", tag, "from", actionList.length, "image(s)")

    if(record) {
      actionHistory.push(actionAddTag.bind(null, refList, tag));
    }
  }

  function recordUserAction() {

  }

  ImageProcessor {
    id: processor
    onImageReady: {
      var ref = ImageDao.findHash(hash)
      var fileName = urlFileName(url)
      var regex = /^([a-zA-Z-_ ]+)[0-9]*\.(\w+)$/
      if(autoTagging && regex.test(fileName)) {
        var result = fileName.match(regex)

        var basename = result[1]
        var extension = result[2]

        var word = /[a-zA-Z]+/g

        var foundTags = basename.match(word)

        console.log("Tags found", foundTags)
        ImageDao.lockWrite()
        ImageDao.transactionStart()
        for(var i in foundTags) {
          ImageDao.addTag(ref, foundTags[i])
        }
        ImageDao.transactionEnd()
        ImageDao.unlockWrite()
      }

      allSimpleList.push(ref)
      viewAppendMultiple([ref])
    }
  }

  onSearchTagsModelChanged: {
    console.log("Update Search Tags")

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
        Layout.fillWidth: true
        Layout.fillHeight: true

        model: allTagModelList
        checkable: true
        onClicked: {
          searchTagsModel = checkedTags
          console.log(checkedTags)
          searchField.text = searchTagsModel.join(' ')
        }
      }

      ComboBox {
        Layout.preferredWidth: 150
        model: imagesPerRowModel
        displayText: "%1 Columns".arg(currentText)
        currentIndex: imagesPerRowModel.indexOf(imagesPerRow)
        onActivated: {
          imagesPerRow = currentText
        }
      }

      CheckBox {
        text: "Crop"
        checked: true
        onClicked: {
          cellFillMode = (cellFillMode == Image.PreserveAspectCrop) ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        }
      }
      ToolButton {
        icon.source: "baseline_settings_white_18dp.png"
        onClicked: {
          settingsLoader.active = true
          settingsLoader.item.open()
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
            actionAddTag(selectionModel, tag)
          }
          addTagPopup.close()
        }
      }
    }
  }

  Image {
    id: offscreen    
    visible: false
    width: renderSize
    height: renderSize
    sourceSize: Qt.size(renderSize, renderSize)
    cache: true
    fillMode: Image.PreserveAspectFit

    property int fileId

    onStatusChanged: if(status == Image.Ready) {
      offscreen.grabToImage(function(result) {
        var hash = ImageDao.hashById(fileId)
        var path = pathPrefix + hash + ".jpg"
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

    onCurrentIndexChanged: {
      if(selectionModel.length <= 1) {
        selectionModel.forEach(function(ref) { ref.selected = false })
      }
      viewModelSimpleList[currentIndex].selected = true
      rebuildSelectionModel()
    }

    property bool isScrolling: false
    property bool showSelectors: toolbar.visible && !list.isScrolling && gridShowSelectors

    onFlickStarted: isScrolling = true
    onFlickEnded: isScrolling = false

    ScrollBar.vertical: ScrollBar {
      onPressedChanged: list.isScrolling = pressed
    }

    Keys.onSpacePressed: {
      lightboxLoader.active = true
      lightboxLoader.item.open()
    }

    Keys.onPressed: {
      if(event.key === Qt.Key_A) {
        addTagPopup.open()
        event.accepted = true
      }
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
        height: imageHeight
        width: imageWidth
        fillMode: cellFillMode
        cache: false
        mipmap: false
        smooth: true
        source: "image://thumper/" + delegateItem.image.fileId
        sourceSize.height: height
        sourceSize.width: width
        opacity: ((selectionModel.length > 1 && !delegateItem.image.selected) ? 0.4 : 1)
        Behavior on opacity {
          NumberAnimation { duration: 100 }
        }

        TapHandler {
          acceptedModifiers: Qt.AltModifier
          onTapped: {
            offscreen.fileId = delegateItem.image.fileId
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
            lightboxLoader.item.open()
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

      // don't even bother loading if the image isn't ready for display
      Loader {
        id: perItemUILoader

        active: false
        property bool shouldProbablyLoad: view.status == Image.Ready && list.showSelectors
        property bool beforeLoad: true

        onShouldProbablyLoadChanged: {
          active = true
          beforeLoad = false
        }

        sourceComponent: CheckBox {
          opacity: (list.showSelectors && !perItemUILoader.beforeLoad) ? 1 : 0

          focusPolicy: Qt.NoFocus
          checked: delegateItem.image.selected
          onClicked: {
            if(delegateItem.image.selected != checked) {
              delegateItem.image.selected = checked
              rebuildSelectionModel()
            }
          }
          Behavior on opacity {
            NumberAnimation { duration: 150 }
          }
        }
      }

      Text {
        visible: gridShowImageIds
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        color: Material.accentColor
        text: delegateItem.image.fileId
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
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 250
        model: selectionTagModelList
        //backgroundColor: Qt.tint('green', Qt.rgba(0, 0, 0, (1 - count / selectionModel.length) * 0.5))

        backgroundColor: 'green'
        onClicked: {
          actionRemoveTag(selectionModel, tag)
        }

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
        Layout.fillWidth: true
        Layout.fillHeight: true

        Layout.preferredWidth: 250
        model: viewTagModelList
        onClicked: {
          actionAddTag(selectionModel, tag)
        }
      }
    }
  }

  Loader {
    id: lightboxLoader
    active: false

    sourceComponent: Lightbox {
      onClosed: lightboxLoader.active = false
    }
  }

  Loader {
    id: settingsLoader
    active: false
    sourceComponent: Settings {
      onClosed: settingsLoader.active = false
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
        processor.downloadList(drop.urls)
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
    sequence: "Ctrl+Z"
    onActivated: {
      console.log("Undo/redo the last action")
      var func = actionHistory.pop()
      func(true)
    }
  }

  Shortcut {
    sequence: "Ctrl+Shift+Z"
    onActivated: {
      if(actionHistory.length) {
        console.log("Undo the last action")
        var func = actionHistory.pop()
        func(false)
      }
    }
  }
}
