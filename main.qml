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
  title: "Thumper 1.10.0"

  FileUtils {
    id: fileUtils
  }

  property var persistentProperties: [
    'pathPrefix',
    'autoTagging',
    'imagesPerRow',
    'renderSize',
    'gridShowImageIds',
    'aspectRatio',
    'cellFillMode',
    'renderPadToFit',
    'renderFilenameToClipboard',
    'duplicateSearchDistance',
  ]

  function loadSettings() {
    var settings = JSON.parse(fileUtils.load("thumper.json"))
    if(settings) {
      for(var k in settings) {
        if(k in root) {
          root[k] = settings[k]
        }
      }
    }
  }

  function saveSettings() {
    var settings = { }
    for(var i in persistentProperties) {
      var k = persistentProperties[i]
      settings[k] = root[k]
    }

    fileUtils.save("thumper.json", JSON.stringify(settings, null, 2))
  }

  Component.onCompleted: {
    loadSettings()
  }

  Component.onDestruction: {
    saveSettings()
  }

  property string pathPrefix: "./"
  property int imagesPerRow: 6
  property var imagesPerRowModel: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 24, 32, 48, 64]
  property int imageWidth: (list.width - spacing) / imagesPerRow - spacing
  property int imageHeight: imageWidth * aspectRatio
  property int spacing: 8
  property int renderSize: -1
  property int cellFillMode: Image.PreserveAspectCrop
  property var renderModel: [ 160, 240, 320, 480, 531, 640, -1]
  property real aspectRatio: 1
  property var aspectRatioModel: [0.5, 0.67, 1.0, 1.5, 2.0]
  property bool autoTagging: false
  property bool renderPadToFit: false
  property bool renderFilenameToClipboard: false
  property bool gridShowImageIds: false
  property int duplicateSearchDistance: 4

  property var viewIdToIndexMap: ({})
  ListModel {
    id: viewModel
  }
  property var viewModelSimpleList: []
  property var allSimpleList: ImageDao.all()

  function imageRefById(fileId) {
    return viewModelSimpleList[viewIdToIndexMap[fileId]]
  }

  property var selectionModel: []
  property var focusSelectionModel: [ viewModelSimpleList[list.currentIndex] ]

  property var effectiveSelectionModel: selectionModel.length > 0 ? selectionModel : focusSelectionModel

  property var selectionTagCount: []
  property var viewTagCount: []

  property var searchTagsModel: []

  QmlTaskListModel { id: selectionTagModelList }
  QmlTaskListModel { id: viewTagModelList }
  QmlTaskListModel {
    id: allTagModelList
    Component.onCompleted: rebuildAllTagModel()
  }

  onEffectiveSelectionModelChanged: rebuildTagModels()

  function rebuildTagModels() {
    selectionTagCount = ImageDao.tagCount(effectiveSelectionModel)
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
      actionHistory.push(actionRemoveTag.bind(null, actionList, tag));
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
      actionHistory.push(actionAddTag.bind(null, actionList, tag));
    }
  }

  function renderImages() {
    var flags = 0
    if(renderPadToFit)
      flags |= ImageDao.PAD_TO_FIT

    if(renderFilenameToClipboard)
      flags |= ImageDao.FNAME_TO_CLIPBOARD

    ImageDao.renderImages(effectiveSelectionModel, pathPrefix, renderSize, flags)
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

  function setViewList(refList) {
    // force the view to reset, this is a performance optimization
    list.model = undefined

    viewClear()
    viewAppendMultiple(refList)
    rebuildSelectionModel()

    list.model = viewModel
  }

  onSearchTagsModelChanged: {
    console.log("Update Search Tags")

    var refList
    if(searchTagsModel.length > 0) {
      refList = ImageDao.search(allSimpleList, searchTagsModel)
    } else {
      refList = allSimpleList
    }   

    setViewList(refList)
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

      ToolButton {
        icon.source: "baseline_save_alt_white_24dp.png"
        onClicked: renderImages()
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
        checked: cellFillMode == Image.PreserveAspectCrop
        onClicked: {
          cellFillMode = checked ? Image.PreserveAspectCrop : Image.PreserveAspectFit
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
            actionAddTag(effectiveSelectionModel, tag)
          }
          addTagPopup.close()
        }
      }
    }
  }

  Component {
    id: highlight
    Rectangle {
      width: list.cellWidth - spacing + border.width * 2
      height: list.cellHeight - spacing + border.width * 2
      color: 'transparent'
      border.color: Material.highlightedButtonColor
      opacity: list.activeFocus ? 1 : 0.5
      border.width: 2
      x: list.currentItem ? list.currentItem.x + list.pad - border.width : 0
      y: list.currentItem ? list.currentItem.y + list.pad - border.width : 0
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

    pixelAligned: false
    interactive: true

    property bool isScrolling: false

    onFlickStarted: isScrolling = true
    onFlickEnded: isScrolling = false

    ScrollBar.vertical: ScrollBar {
      onPressedChanged: list.isScrolling = pressed
    }

    Keys.onPressed: {
      if(event.key === Qt.Key_A) {
        addTagPopup.open()
        event.accepted = true
      } else if(event.key === Qt.Key_F) {
        lightboxLoader.active = true
        lightboxLoader.item.open()
        event.accepted = true
      } else if(event.key === Qt.Key_R) {
        renderImages()
        event.accepted = true
      }
    }

    function selectRange(left, right, selected) {
      var min = Math.min(left, right)
      var max = Math.max(left, right)

      for(var i = min; i <= max; i++) {
        viewModelSimpleList[i].selected = selected
      }
    }

    Keys.onSpacePressed: {
      var ref = viewModelSimpleList[currentIndex]
      ref.selected = !ref.selected
      rebuildSelectionModel()
    }

    delegate: Item {
      id: delegateItem
      width: list.cellWidth
      height: list.cellHeight

      property ImageRef image: ref

      BorderImage {
        source: "selection_box.png"
        visible: delegateItem.image.selected
        anchors.margins: list.pad - 2
        anchors.fill: parent
        border.left: 3; border.top: 3
        border.right: 3; border.bottom: 3
        horizontalTileMode: BorderImage.Repeat
        verticalTileMode: BorderImage.Repeat
        smooth: false
        cache: true
      }

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
        opacity: ((selectionModel.length > 0 && !delegateItem.image.selected) ? 0.5 : 1)
        Behavior on opacity {
          NumberAnimation { duration: 100 }
        }

        TapHandler {
          acceptedModifiers: Qt.ShiftModifier
          onTapped: {
            list.selectRange(list.currentIndex, index, true)

            list.forceActiveFocus()
            list.currentIndex = index
            rebuildSelectionModel()
          }
        }

        TapHandler {
          acceptedModifiers: Qt.ShiftModifier | Qt.ControlModifier
          onTapped: {
            list.selectRange(list.currentIndex, index, false)

            list.forceActiveFocus()
            list.currentIndex = index
            rebuildSelectionModel()
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
            list.forceActiveFocus()
            list.currentIndex = index

            delegateItem.image.selected = !delegateItem.image.selected
            rebuildSelectionModel()
          }
        }
      }

      Rectangle {
        visible: gridShowImageIds
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        //anchors.bottomMargin: 4

        width: labelText.implicitWidth + 8
        height: labelText.implicitHeight + 4

        color: '#80000000'

        Text {
          x: 4
          y: 2
          id: labelText

          color: Material.accentColor
          text: "%1 %2x%3".arg(delegateItem.image.fileId)
                            .arg(delegateItem.image.size.width)
                            .arg(delegateItem.image.size.height)
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
        text: "Selected %1/%2".arg(effectiveSelectionModel.length).arg(viewModel.count)
      }

      TagList {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 250
        model: selectionTagModelList
        //backgroundColor: Qt.tint('green', Qt.rgba(0, 0, 0, (1 - count / selectionModel.length) * 0.5))

        backgroundColor: 'green'
        onClicked: {
          actionRemoveTag(effectiveSelectionModel, tag)
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
          actionAddTag(effectiveSelectionModel, tag)
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
