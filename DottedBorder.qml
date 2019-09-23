import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.12
import QtQml 2.12
import thumper 1.0

Item {
  Image {
    // top bar
    anchors.rightMargin: 2
    anchors.top: parent.top
    anchors.left: parent.left
    anchors.right: parent.right
    height: 2
    fillMode: Image.Tile
    verticalAlignment: Image.AlignTop
    source: 'checkboard.png'
    cache: true
    smooth: false
  }
  
  Image {
    // right bar
    anchors.topMargin: 2
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    width: 2
    fillMode: Image.Tile
    horizontalAlignment: Image.AlignLeft
    source: 'checkboard.png'
    cache: true
    smooth: false
  }
  
  Image {
    // bottom bar
    anchors.leftMargin: 2
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    height: 2
    fillMode: Image.Tile
    verticalAlignment: Image.AlignBottom
    source: 'checkboard.png'
    cache: true
    smooth: false
  }
  
  Image {
    // left bar
    anchors.bottomMargin: 2
    anchors.left: parent.left
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    width: 2
    fillMode: Image.Tile
    horizontalAlignment: Image.AlignLeft
    source: 'checkboard.png'
    cache: true
    smooth: false
  }
}
