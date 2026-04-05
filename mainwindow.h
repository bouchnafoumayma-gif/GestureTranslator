#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoWidget>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void processFrame(const QVideoFrame &frame);

private:
    Ui::MainWindow *ui;
    QCamera *camera;
    QMediaCaptureSession *session;
    QVideoWidget *videoWidget;
    QVideoSink *videoSink;
    QImage currentFrame;

    // Ces deux fonctions doivent être déclarées ici !
    QString detectGesture(const QImage &image);
    int countSkinPixels(const QImage &image);
};

#endif