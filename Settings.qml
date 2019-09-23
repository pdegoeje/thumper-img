import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import thumper 1.0

Popup {
  id: root

  anchors.centerIn: Overlay.overlay
  parent: Overlay.overlay
  modal: true

  width: 640
  height: 480
  padding: 0

  ColumnLayout {
    anchors.fill: parent

    TabBar {
      Layout.fillWidth: true

      id: tabBar
      TabButton {
        text: "General"
      }

      TabButton {
        text: "Import"
      }

      TabButton {
        text: "Export"
      }

      TabButton {
        text: "Help"
      }
    }

    StackLayout {
      Layout.margins: 10
      Layout.fillHeight: true
      Layout.fillWidth: true
      currentIndex: tabBar.currentIndex
      ColumnLayout {
        Button {
          text: "Open Image Database"
          onClicked: fileUtils.openImageDatabase()
        }

        RowLayout {
          Label {
            text: "Aspect ratio"
          }

          ComboBox {
            model: aspectRatioModel
            displayText: "1:%1".arg(currentText)
            currentIndex: aspectRatioModel.indexOf(aspectRatio)
            onActivated: {
              aspectRatio = currentText
            }
          }
        }

        Switch {
          checked: gridShowImageIds
          text: "Show image IDs"
          onClicked: gridShowImageIds = checked
        }

        RowLayout {
          Button {
            id: fixMetaData
            ImageProcessStatus {
              id: statusUpdate
              onUpdate: {
                fixStatus.text = "Processed %1 images".arg((fractionComplete).toFixed(0))
              }
              onComplete: {
                fixStatus.text = "Complete"
              }
            }

            text: "Fix image metadata"
            onClicked: ImageDao.fixImageMetaData(statusUpdate)
          }
          Label {
            id: fixStatus
          }
        }

        RowLayout {
          Button {
            text: "Find duplicates"
            onClicked: {
              var refList = ImageDao.findAllDuplicates(allSimpleList, maxDistance.value)
              setViewList(refList)
            }
          }
          Label {
            text: "Sloppyness %1".arg(duplicateSearchDistance)
          }

          Slider {
            id: maxDistance
            from: 0
            to: 20
            value: duplicateSearchDistance
            stepSize: 1
            onMoved: duplicateSearchDistance = value
          }
        }

        Switch {
          text: "Show images marked for removal"
          checked: showHiddenImages
          onClicked: showHiddenImages = checked
        }

        Button {
          text: "Delete images marked for removal"
          onClicked: {
            ImageDao.purgeDeletedImages()
          }
        }
      }

      ColumnLayout {
        Switch {
          checked: autoTagging
          text: "Automatically tag imported images"
          onClicked: autoTagging = checked
        }
      }

      ColumnLayout {
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
            displayText: currentText
            currentIndex: renderModel.indexOf(renderSize)
            onActivated: {
              renderSize = currentText
            }
          }
        }

        Switch {
          text: "Pad image to fit size"
          checked: renderPadToFit
          onClicked: renderPadToFit = checked
        }

        Switch {
          text: "Copy rendered image filenames to clipboard"
          checked: renderFilenameToClipboard
          onClicked: renderFilenameToClipboard = checked
        }
      }

      ColumnLayout {
        Label {
          textFormat: Text.StyledText
          text:
              "<b>Shortcuts</b><br><ul>" +
              "<li>Click: Focus image</li>" +
              "<li>Ctrl+Click: Toggle selection</li>" +
              "<li>Shift+Click: Select multiple</li>" +
              "<li>Ctrl+Shift+Click: Deselect multiple</li>" +
              "<li>Space: Toggle selection of focused image</li>" +
              "<li>Ctrl+A: Select all" +
              "<li>Ctrl+D: Deselect all" +
              "<li>Ctrl+I: Invert selection" +
              "<li>Ctrl+Z: Undo/Redo" +
              "<li>Ctrl+Shift+Z: Undo" +
              "<li>F: Enlarge focused image</li>" +
              "<li>A: Add (new) tag to selection</li>" +
              "<li>R: Export selection</li>" +
              "<li>Delete: Mark selection for removal</li>" +
              "<li>Insert: Keep selection</li>" +
              "</ul>"
        }
      }
    }
  }
}
