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
  height: 512
  padding: 0

  ColumnLayout {
    anchors.fill: parent

    TabBar {
      Layout.fillWidth: true

      id: tabBar
      TabButton {
        text: "View"
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
      TabButton {
        text: "Misc."
      }

    }

    StackLayout {
      Layout.margins: 10
      Layout.fillHeight: true
      Layout.fillWidth: true
      currentIndex: tabBar.currentIndex
      ColumnLayout {
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

        Switch {
          text: "Show images marked for removal"
          checked: showHiddenImages
          onClicked: showHiddenImages = checked
        }

        Switch {
          text: "Zoom when hovering over images"
          checked: zoomOnHover
          onClicked: zoomOnHover = checked
        }


        RowLayout {
          Slider {
            from: 2
            to: 320
            value: imageSourceMinSize
            stepSize: 2
            onMoved: imageSourceMinSize = value
          }
          Label {
            text: "Hover zoom size: %1 pixels".arg(imageSourceMinSize)
          }
        }

        Button {
          text: "Find duplicates"
          onClicked: {
            var refList = ImageDao.findAllDuplicates(allSimpleList, maxDistance.value)
            setViewList(refList)
          }
        }
        RowLayout {
          Slider {
            id: maxDistance
            from: 0
            to: 20
            value: duplicateSearchDistance
            stepSize: 1
            onMoved: duplicateSearchDistance = value
          }
          Label {
            text: "Match accuracy: %1".arg(duplicateSearchDistance)
          }
        }

        RowLayout {
          Slider {
            id: spacingSlider
            from: 0
            to: 16
            value: window.spacing
            stepSize: 1
            onMoved: window.spacing = value
          }
          Label {
            text: "Space between images: %1".arg(window.spacing)
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

      ColumnLayout {
        Button {
          text: "Open Image Database"
          onClicked: fileUtils.openImageDatabase()
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

        Button {
          text: "Delete images marked for removal"
          onClicked: {
            ImageDao.purgeDeletedImages()
          }
        }
      }
    }
  }
}
