#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMediaDevices>
#include <QPixmap>
#include <opencv2/imgproc.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Traducteur de Gestes");

    ui->cameraLabel->setScaledContents(true);

    QList<QCameraDevice> camList = QMediaDevices::videoInputs();
    if (!camList.isEmpty()) {
        QCameraDevice selectedCam = camList.first();
        for (const QCameraDevice &cam : camList) {
            if (cam.description().contains("Integrated Camera", Qt::CaseInsensitive)) {
                selectedCam = cam;
                break;
            }
        }
        camera = new QCamera(selectedCam, this);
    } else {
        camera = new QCamera(this);
    }

    session = new QMediaCaptureSession(this);
    session->setCamera(camera);

    videoSink = new QVideoSink(this);
    session->setVideoSink(videoSink);

    connect(videoSink, &QVideoSink::videoFrameChanged,
            this, &MainWindow::processFrame);

    camera->start();
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->gestureLabel->setText("Mets ta main dans le rectangle !");
    ui->translationLabel->setText("Traduction : —");
}

MainWindow::~MainWindow()
{
    camera->stop();
    delete ui;
}

cv::Mat QImageToMat(const QImage &image)
{
    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(converted.height(), converted.width(),
                CV_8UC3,
                const_cast<uchar*>(converted.bits()),
                converted.bytesPerLine());
    cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    return mat.clone();
}

QImage MatToQImage(const cv::Mat &mat)
{
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows,
                  rgb.step, QImage::Format_RGB888).copy();
}

int MainWindow::countFingers(const cv::Mat &handRegion)
{
    cv::Mat gray;
    cv::cvtColor(handRegion, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

    cv::Mat binary;
    cv::threshold(gray, binary, 0, 255,
                  cv::THRESH_BINARY + cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(binary, binary, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours,
                     cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return -1;

    int maxIdx = 0;
    double maxArea = 0;
    for (int i = 0; i < (int)contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) {
            maxArea = area;
            maxIdx = i;
        }
    }

    if (maxArea < 2000) return -1;

    std::vector<int> hullIdx;
    cv::convexHull(contours[maxIdx], hullIdx);

    if (hullIdx.size() <= 3) return 0;

    std::vector<cv::Vec4i> defects;
    cv::convexityDefects(contours[maxIdx], hullIdx, defects);

    int fingers = 0;
    for (const cv::Vec4i &defect : defects) {
        float depth = defect[3] / 256.0f;
        if (depth > 20) fingers++;
    }

    return fingers;
}

QString MainWindow::detectGesture(const cv::Mat &frame)
{
    if (frame.empty()) return "aucun";

    int cx = frame.cols / 2;
    int cy = frame.rows / 2;
    int w  = frame.cols / 3;
    int h  = frame.rows / 2;

    cv::Rect zone(cx - w/2, cy - h/2, w, h);
    if (zone.x < 0 || zone.y < 0 ||
        zone.x + zone.width  > frame.cols ||
        zone.y + zone.height > frame.rows) {
        return "aucun";
    }

    cv::Mat handRegion = frame(zone);
    int fingers = countFingers(handRegion);

    if (fingers == -1) return "aucun";
    if (fingers == 0)  return "poing";
    if (fingers == 1)  return "un_doigt";
    if (fingers == 2)  return "deux_doigts";
    if (fingers == 3)  return "trois_doigts";
    if (fingers >= 4)  return "main_ouverte";

    return "aucun";
}

void MainWindow::processFrame(const QVideoFrame &frame)
{
    currentFrame = frame.toImage();
    if (currentFrame.isNull()) return;

    cv::Mat mat = QImageToMat(currentFrame);
    if (mat.empty()) return;

    int cx = mat.cols / 2;
    int cy = mat.rows / 2;
    int w  = mat.cols / 3;
    int h  = mat.rows / 2;

    cv::rectangle(mat,
                  cv::Point(cx - w/2, cy - h/2),
                  cv::Point(cx + w/2, cy + h/2),
                  cv::Scalar(0, 255, 0), 2);

    QImage display = MatToQImage(mat);
    ui->cameraLabel->setPixmap(QPixmap::fromImage(display));

    QString gesture = detectGesture(mat);

    if (gesture == "poing") {
        ui->gestureLabel->setText("Geste : Poing ✊");
        ui->gestureLabel->setStyleSheet("color: blue; font-size: 14px;");
        ui->translationLabel->setText("Merci !");
        ui->translationLabel->setStyleSheet(
            "color: white; background-color: blue;"
            "border-radius: 10px; font-size: 18px; font-weight: bold;");
    } else if (gesture == "un_doigt") {
        ui->gestureLabel->setText("Geste : Un doigt ☝️");
        ui->gestureLabel->setStyleSheet("color: green; font-size: 14px;");
        ui->translationLabel->setText("Oui !");
        ui->translationLabel->setStyleSheet(
            "color: white; background-color: green;"
            "border-radius: 10px; font-size: 18px; font-weight: bold;");
    } else if (gesture == "deux_doigts") {
        ui->gestureLabel->setText("Geste : Deux doigts ✌️");
        ui->gestureLabel->setStyleSheet("color: red; font-size: 14px;");
        ui->translationLabel->setText("Non !");
        ui->translationLabel->setStyleSheet(
            "color: white; background-color: red;"
            "border-radius: 10px; font-size: 18px; font-weight: bold;");
    } else if (gesture == "trois_doigts") {
        ui->gestureLabel->setText("Geste : Trois doigts 🤟");
        ui->gestureLabel->setStyleSheet("color: orange; font-size: 14px;");
        ui->translationLabel->setText("S'il vous plaît !");
        ui->translationLabel->setStyleSheet(
            "color: white; background-color: orange;"
            "border-radius: 10px; font-size: 18px; font-weight: bold;");
    } else if (gesture == "main_ouverte") {
        ui->gestureLabel->setText("Geste : Main ouverte ✋");
        ui->gestureLabel->setStyleSheet("color: purple; font-size: 14px;");
        ui->translationLabel->setText("Bonjour !");
        ui->translationLabel->setStyleSheet(
            "color: white; background-color: purple;"
            "border-radius: 10px; font-size: 18px; font-weight: bold;");
    } else {
        ui->gestureLabel->setText("Mets ta main dans le rectangle !");
        ui->gestureLabel->setStyleSheet("color: gray; font-size: 14px;");
        ui->translationLabel->setText("—");
        ui->translationLabel->setStyleSheet(
            "color: gray; background-color: transparent;"
            "font-size: 18px;");
    }
}

void MainWindow::on_startButton_clicked()
{
    camera->start();
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->gestureLabel->setText("Mets ta main dans le rectangle !");
}

void MainWindow::on_stopButton_clicked()
{
    camera->stop();
    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);
    ui->gestureLabel->setText("Geste : arrêté");
}