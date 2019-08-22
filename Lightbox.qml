import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.12
import QtQml 2.12
import thumper 1.0

Popup {
  id: lightbox
  
  /*Shortcut {
    sequence: "Escape"
    onActivated: close()
  }*/
  
  Shortcut {
    sequence: "Space"
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
