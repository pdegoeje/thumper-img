import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Material
import QtQml
import thumper

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
