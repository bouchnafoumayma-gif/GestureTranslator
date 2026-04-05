#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>
#include <QLabel>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

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
    QVideoSink *videoSink;
    QImage currentFrame;

    QString detectGesture(const cv::Mat &frame);
    int countFingers(const cv::Mat &handRegion);
};

#endif