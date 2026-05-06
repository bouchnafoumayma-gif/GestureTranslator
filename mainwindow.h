#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>
#include <QLabel>
#include <QTimer> // Ajouté pour QTimer*
#include <QSqlDatabase>
#include <QSqlQuery>
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
    void on_sosButton_clicked();
    void on_apprentissageToggle_clicked();
    void validerGeste();

private:
    Ui::MainWindow *ui;

    // Caméra et flux
    QCamera *camera;
    QMediaCaptureSession *session;
    QVideoSink *videoSink;
    QImage currentFrame;

    // États et modes
    bool isUrgencyMode;
    bool isApprentissageMode = false;
    QString gesteCible;
    QTimer *timerApprentissage;
    int progressCount = 0;

    // SQL
    QSqlDatabase db;
    void initDatabase();
    QString getTranslation(const QString &gesture);

    // Traitement d'image (OpenCV)
    QString detectGesture(const cv::Mat &frame);
    int countFingers(const cv::Mat &handRegion);

    // Stabilité
    QString lastGesture;
    int stabilityCounter = 0;
};

#endif // MAINWINDOW_H