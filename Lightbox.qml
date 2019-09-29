import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.12
import QtQml 2.12
import thumper 1.0

Popup {
  id: lightbox
  
  Shortcut {
    sequence: "F"
    onActivated: close()
  }

  property ImageRef image: viewModelSimpleList[list.currentIndex]
  
  parent: Overlay.overlay
  anchors.centerIn: Overlay.overlay
  modal: true
  padding: 0
  
  onClosed: lightboxLoader.active = false
  
  Overlay.modal: Rectangle {
    color: '#CC101010'
    Behavior on opacity { NumberAnimation { duration: 150 } }
  }
  
  background: Item {
    MouseArea {
      anchors.fill: parent
      onClicked: lightbox.close()
    }
  }
  
  Flickable {
    id: flickable
    implicitWidth: Math.min(window.width, contentWidth)
    implicitHeight: Math.min(window.height, contentHeight)

    contentWidth: content.width
    contentHeight: content.height
    
    synchronousDrag: true
    boundsBehavior: Flickable.StopAtBounds

    focus: true

    Keys.onUpPressed: list.moveCurrentIndexUp()
    Keys.onDownPressed: list.moveCurrentIndexDown()
    Keys.onLeftPressed: list.moveCurrentIndexLeft()
    Keys.onRightPressed: list.moveCurrentIndexRight()
    Keys.onPressed: {
      var oX = contentX + width / 2
      var oY = contentY + height / 2

      var zoom = 1

      if(event.key === Qt.Key_BracketRight) {
        zoom = 2
      } else if(event.key === Qt.Key_BracketLeft) {
        zoom = 0.5
      }
      content.width *= zoom
      content.height *= zoom
      oX *= zoom
      oY *= zoom

      contentX = oX - width / 2
      contentY = oY - height / 2
    }

    Image {
      id: content
      
      opacity: (status == Image.Ready) ? 1 : 0
      
      asynchronous: true
      smooth: true
      mipmap: true
      source: "image://thumper/" + lightbox.image.fileId
      
      onStatusChanged: {
        if(status === Image.Ready) {
          var scaleX = implicitWidth / window.width
          var scaleY = implicitHeight / window.height

          var scaleF = scaleX < scaleY ? scaleX : scaleY
          if(scaleF < 1) {
            scaleF = 1
          }

          width = implicitWidth / scaleF
          height = implicitHeight / scaleF

          flickable.contentX = (width - flickable.implicitWidth) / 2
          flickable.contentY = (height - flickable.implicitHeight) / 2

          flickable.forceActiveFocus()
        }
      }

      Behavior on opacity {
        OpacityAnimator { duration: 100 }
      }
    }
  }
}
