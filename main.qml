import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.12
import QtQml 2.12
import thumper 1.0

ApplicationWindow {
  id: window
  visible: true
  width: 1280
  height: 720
  title: "Thumper 1.17.0"

  FileUtils {
    id: fileUtils
    onImageDatabaseSelected: {
      console.log("fileselected: ", file)
      launchThumper(file)
    }
  }

  readonly property string settingsFilename: thumper.databaseFilename + ".config.json"

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
    'showHiddenImages',
    'spacing',
    'zoomOnHover',
    'imageSourceMinSize',
    'imageOverlayFormat',
  ]

  function loadSettings() {
    var file = fileUtils.load(settingsFilename)
    if(file) {
      Object.assign(this, JSON.parse(file))
    }
  }

  function saveSettings() {
    var settings = { }
    for(var i in persistentProperties) {
      var k = persistentProperties[i]
      settings[k] = window[k]
    }

    fileUtils.save(settingsFilename, JSON.stringify(settings, null, 2))
  }

  Component.onCompleted: {    
    loadSettings()
    allSimpleList = ImageDao.all(showHiddenImages)
    setViewList(allSimpleList)
  }

  Component.onDestruction: {
    saveSettings()
  }

  property string pathPrefix: ""

  property int imagesPerRow: 6
  property var imagesPerRowModel: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 24, 32, 48, 64]

  property int imageCellWidth: (window.width - spacing) / imagesPerRow
  property int imageCellHeight: (imageCellWidth - spacing) * aspectRatio + spacing

  property int imageWidth: Math.floor((imageCellWidth - spacing) / 2) * 2
  property int imageHeight: Math.floor((imageCellHeight - spacing) / 2) * 2

  property int imageSourceMinSize: 32
  property int imageSourceWidth: imageWidth < imageSourceMinSize ? imageSourceMinSize : imageWidth
  property int imageSourceHeight: imageHeight < imageSourceMinSize ? imageSourceMinSize : imageHeight

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
  property bool showHiddenImages: false
  property bool zoomOnHover: true
  property string imageOverlayFormat: "$id$\n$width$x$height$ $size$KB $format$\n$tags$"

  ListModel {
    id: viewModel
  }

  // lists of ImageRefs
  property var viewModelSimpleList: []
  property var allSimpleList: []
  property var selectionModel: []
  property var effectiveSelectionModel: {
    if(selectionModel.length > 0) {
      return selectionModel
    } else if(viewModelSimpleList.length > 0) {
      return [ viewModelSimpleList[list.currentIndex] ]
    } else {
      return []
    }
  }

  property var selectionTagCount: []
  property var viewTagCount: []

  property var searchTagsModel: []

  QmlTaskListModel { id: selectionTagModelList }
  QmlTaskListModel { id: viewTagModelList }
  QmlTaskListModel {
    id: allTagModelList
    Component.onCompleted: rebuildAllTagModel()
  }

  onEffectiveSelectionModelChanged: {
    rebuildTagModels()
  }

  function rebuildTagModels() {
    selectionTagCount = ImageDao.tagCount(effectiveSelectionModel)
    viewTagCount = ImageDao.tagCount(viewModelSimpleList)

    selectionTagModelList.update(selectionTagCount)
    viewTagModelList.update(viewTagCount)
  }

  function rebuildAllTagModel() {
    var allTagCount = ImageDao.tagCount(allSimpleList)
    allTagModelList.update(allTagCount)
  }

  function rebuildSelectionModel() {
    selectionModel = viewModelSimpleList.filter(function(ref) { return ref.selected })
  }

  function viewClear() {
    viewModel.clear()
    viewModelSimpleList = []
  }

  function viewAppendMultiple(refList) {
    var modelRefList = []
    for(var i = 0; i < refList.length; i++) {
      var ref = refList[i]
      viewModelSimpleList.push(ref)
      modelRefList.push({ 'ref' : ref })
    }

    viewModel.append(modelRefList)
  }

  property var actionHistory: []

  function actionAddTag(refList, tag, record = true) {
    console.log("actionAddTag")
    var actionList = []

    actionList = ImageDao.addTag(refList, tag)
    rebuildTagModels()
    rebuildAllTagModel()

    console.log("Added tag", tag, "to", actionList.length, "image(s)")

    if(record && actionList.length > 0) {
      actionHistory.push(actionRemoveTag.bind(null, actionList, tag));
    }
  }


  function actionRemoveTag(refList, tag, record = true) {
    console.log("actionAddTag")
    var actionList = []

    actionList = ImageDao.removeTag(refList, tag);
    rebuildTagModels()
    rebuildAllTagModel()

    console.log("Removed tag", tag, "from", actionList.length, "image(s)")

    if(record && actionList.length > 0) {
      actionHistory.push(actionAddTag.bind(null, actionList, tag));
    }
  }

  function actionDelete(refList, record = true) {
    var actionList = []

    actionList = ImageDao.updateDeleted(refList, true);
    console.log("Deleted", actionList.length, "image(s)")

    if(record && actionList.length > 0) {
      actionHistory.push(actionUndelete.bind(null, actionList));
    }
  }

  function actionUndelete(refList, record = true) {
    var actionList = []

    actionList = ImageDao.updateDeleted(refList, false);
    console.log("Undeleted", actionList.length, "image(s)")

    if(record && actionList.length > 0) {
      actionHistory.push(actionDelete.bind(null, actionList));
    }
  }

  function renderImages() {
    var flags = 0
    if(renderPadToFit)
      flags |= ImageDao.PAD_TO_FIT

    if(renderFilenameToClipboard)
      flags |= ImageDao.FNAME_TO_CLIPBOARD

    ImageDao.renderImages(effectiveSelectionModel, thumper.resolveRelativePath(pathPrefix), renderSize, flags)
  }

  ImageProcessor {
    id: processor
    onImageReady: {
      var ref = ImageDao.createImageRef(fileId)
      var fileName = urlFileName(url)
      var regex = /^([a-zA-Z-_ ]+)[0-9]*\.(\w+)$/
      if(autoTagging && regex.test(fileName)) {
        var result = fileName.match(regex)

        var basename = result[1]
        var extension = result[2]

        var word = /[a-zA-Z]+/g

        var foundTags = basename.match(word)

        console.log("Tags found", foundTags)        
        for(var i in foundTags) {
          ImageDao.addTag([ref], foundTags[i])
        }
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
    console.log("onSearchTagsModelChanged")

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
        icon.source: "baseline_folder_open_white_24dp.png"
        onClicked: fileUtils.openImageDatabase()
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
      width: imageWidth //list.cellWidth - spacing + border.width * 2
      height: imageHeight //list.cellHeight - spacing + border.width * 2
      color: 'transparent'
      border.color: Material.highlightedButtonColor
      opacity: list.activeFocus ? 1 : 0.5
      border.width: 2
      x: list.currentItem ? list.currentItem.x + list.pad : 0
      y: list.currentItem ? list.currentItem.y + list.pad : 0
      z: 2
    }
  }

  GridView {
    activeFocusOnTab: true
    anchors.fill: parent
    anchors.leftMargin: Math.floor((window.width - imageCellWidth * imagesPerRow) / 2)

    id: list

    focus: true

    model: viewModel
    property int pad: spacing / 2
    property int lastRowIndex: Math.floor((viewModel.count - 1) / imagesPerRow) * imagesPerRow

    topMargin: pad
    bottomMargin: pad
    leftMargin: 0//pad
    rightMargin: pad

    cellWidth: imageCellWidth
    cellHeight: imageCellHeight

    highlight: highlight
    highlightFollowsCurrentItem: false

    pixelAligned: false
    interactive: !lightboxLoader.active

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
      } else if(event.key === Qt.Key_Delete) {
        actionDelete(effectiveSelectionModel)
        event.accepted = true
      } else if(event.key === Qt.Key_Insert) {
        actionUndelete(effectiveSelectionModel)
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
      z: view.isZoomed ? 1 : 0

      property ImageRef image: ref
      property int column: index % imagesPerRow

      HoverHandler {
        id: hoverHandler
        enabled: zoomOnHover
      }

      Binding {
        target: image
        when: gridShowImageIds
        property: "overlayFormat"
        value: imageOverlayFormat
      }

      Image {
        x: {
          if(delegateItem.column == 0) {
            return list.pad
          } else if(delegateItem.column == imagesPerRow - 1) {
            return list.pad + imageWidth - width
          } else {
            return list.pad + (imageWidth - width) / 2;
          }
        }
        y: {
          if(index < imagesPerRow) {
            return list.pad
          } else if(index >= list.lastRowIndex) {
            return list.pad + imageHeight - height
          } else {
            return list.pad + (imageHeight - height) / 2;
          }
        }

        id: view

        property int halfWidth: (hoverHandler.hovered ? implicitWidth : imageWidth) / 2
        property int halfHeight: (hoverHandler.hovered ? implicitHeight : imageHeight) / 2
        property bool isZoomed: height > imageHeight || width > imageWidth

        width: halfWidth * 2
        height: halfHeight * 2
        fillMode: cellFillMode
        asynchronous: true
        cache: false
        mipmap: false
        smooth: true
        sourceSize.width: imageSourceWidth
        sourceSize.height: imageSourceHeight
        source: "image://thumper/" + delegateItem.image.fileId
        opacity: ((selectionModel.length > 0 && !delegateItem.image.selected) ? 0.5 : 1)

        Behavior on halfWidth {
          SmoothedAnimation { duration: 100; easing.type: Easing.InOutCubic }
        }

        Behavior on halfHeight {
          SmoothedAnimation { duration: 100; easing.type: Easing.InOutCubic }
        }

        Behavior on opacity {
          NumberAnimation { duration: 100;  }
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
            if(selectionModel.length == 0) {
              viewModelSimpleList[list.currentIndex].selected = true
            }

            list.currentIndex = index
            delegateItem.image.selected = !delegateItem.image.selected
            rebuildSelectionModel()
          }
        }

        Rectangle {
          visible: hoverHandler.hovered
          anchors.fill: parent
          anchors.margins: -spacing
          border.color: '#FF303030'
          border.width: spacing
          color: 'transparent'
        }

        DottedBorder {
          anchors.fill: parent
          visible: delegateItem.image.selected
        }
      }

      Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        visible: delegateItem.image.deleted
        radius: 5
        width: 10
        height: 10
        color: 'red'
      }

      Text {
        anchors.fill: parent
        anchors.margins: 10

        visible: gridShowImageIds
        id: labelText

        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        elide: Text.ElideRight

        font.pixelSize: 18

        color: Material.accentColor
        text: delegateItem.image.overlayString
        style: Text.Outline
        styleColor: '#FF000000'
      }
    }
  }

  TagSelection {
    id: tagSelection

    onEditComplete: {
      var result = ImageDao.search(viewModelSimpleList, selectedTags)
      if(result.length > 0) {
        viewModelSimpleList.forEach(function(ref) { ref.selected = false })

        var indices = []

        result.forEach(function(ref, index) { ref.selected = true; indices.push(index) })
        rebuildSelectionModel()

        if(indices.length > 0) {
          list.positionViewAtIndex(indices[0], GridView.Center)
        }
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

      BusyIndicator {
        parent: Overlay.overlay
        anchors.centerIn: parent
        id: busyIndicator
        running: ImageDao.busy
        z: 1000
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
    keys: ["text/plain", "text/uri-list"]

    onDropped: {
      if(drop.hasText || drop.hasUrls) {
        drop.acceptProposedAction()

        var urilist = processor.parseTextUriList(drop.text)
        if(urilist.length > 0) {
          processor.downloadList(urilist)
        } else {
          processor.downloadList(drop.urls)
        }
      }
    }
  }

  Shortcut {
    sequence: StandardKey.FullScreen
    onActivated: {
      window.visibility = (window.visibility == Window.FullScreen) ? Window.Windowed : Window.FullScreen
      console.log("Toggle Fullscreen")
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
      viewModelSimpleList.forEach(function(ref) { ref.selected = false })
      rebuildSelectionModel()
    }
  }

  Shortcut {
    sequence: "Ctrl+A"
    onActivated: {
      console.log("Select all")
      viewModelSimpleList.forEach(function(ref) { ref.selected = true })
      rebuildSelectionModel()
    }
  }

  Shortcut {
    sequence: "Ctrl+I"
    onActivated: {
      console.log("Invert Selection")
      viewModelSimpleList.forEach(function(ref) { ref.selected = !ref.selected })
      rebuildSelectionModel()
    }
  }

  Shortcut {
    sequence: "Ctrl+Z"
    onActivated: {
      if(actionHistory.length) {
        console.log("Undo/redo the last action")
        var func = actionHistory.pop()
        func(true)
      }
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

  Shortcut {
    sequence: "F5"
    onActivated: {
      console.log(Material.backgroundColor)
    }
  }
}
