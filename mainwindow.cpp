#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMediaDevices>
#include <QPixmap>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <cmath>
#include <opencv2/imgproc.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 1. Initialisation de la Base de Données
    db = QSqlDatabase::addDatabase("QSQLITE");
    // Chemin vers ton fichier "gestures" dans le dossier de build
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/gestures.db");

    if (!db.open()) {
        qDebug() << "Erreur : Impossible d'ouvrir la base de données" << db.lastError().text();
    } else {
        qDebug() << "Connexion réussie à la base gestures (avec colonne contexte).";
        // Note : On ne fait plus de CREATE TABLE ou d'INSERT ici
        // car tu l'as déjà fait proprement dans DB Browser.
        qDebug() << "Le chemin reel utilise par Qt est :" << db.databaseName();
    }

    // 2. Configuration de l'Interface et Caméra
    setWindowTitle("GestureTranslator - ENSAH");
    ui->cameraLabel->setScaledContents(true);

    // Initialisation de la liste déroulante (QComboBox) pour les contextes
    // Assure-toi que l'objet s'appelle 'contexteCombo' dans Qt Designer
    if(ui->contexteCombo->count() == 0) {
        ui->contexteCombo->addItems({"General", "Santé", "Restaurant"});
    }
    // Dans le constructeur, après l'initialisation de contexteCombo
    if(ui->langueCombo->count() == 0) {
        ui->langueCombo->addItems({"FR", "EN", "AR"});
    }

    QList<QCameraDevice> camList = QMediaDevices::videoInputs();
    camera = (!camList.isEmpty()) ? new QCamera(camList.first(), this) : new QCamera(this);

    session = new QMediaCaptureSession(this);
    session->setCamera(camera);
    videoSink = new QVideoSink(this);
    session->setVideoSink(videoSink);

    connect(videoSink, &QVideoSink::videoFrameChanged, this, &MainWindow::processFrame);

    camera->start();
    ui->startButton->setEnabled(false);
}
MainWindow::~MainWindow() {
    camera->stop();
    delete ui;
}

// --- Fonctions de conversion utilitaires ---
cv::Mat QImageToMat(const QImage &image) {
    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(converted.height(), converted.width(), CV_8UC3,
                const_cast<uchar*>(converted.bits()), converted.bytesPerLine());
    cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    return mat.clone();
}

QImage MatToQImage(const cv::Mat &mat) {
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}

// --- ALGORITHME AMÉLIORÉ (Loi des Cosinus) ---
int MainWindow::countFingers(const cv::Mat &handRegion) {
    cv::Mat gray, binary;
    cv::cvtColor(handRegion, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return -1;

    int maxIdx = 0;
    double maxArea = 0;
    for (int i = 0; i < (int)contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) { maxArea = area; maxIdx = i; }
    }

    if (maxArea < 3000) return -1;

    std::vector<int> hullIdx;
    std::vector<cv::Vec4i> defects;
    cv::convexHull(contours[maxIdx], hullIdx, false);

    if (hullIdx.size() < 3) return 0;
    cv::convexityDefects(contours[maxIdx], hullIdx, defects);

    int fingers = 0;
    for (const cv::Vec4i &defect : defects) {
        cv::Point start = contours[maxIdx][defect[0]];
        cv::Point end   = contours[maxIdx][defect[1]];
        cv::Point far   = contours[maxIdx][defect[2]];

        // Loi des cosinus pour valider l'angle entre les doigts
        double a = std::sqrt(std::pow(end.x - start.x, 2) + std::pow(end.y - start.y, 2));
        double b = std::sqrt(std::pow(far.x - start.x, 2) + std::pow(far.y - start.y, 2));
        double c = std::sqrt(std::pow(end.x - far.x, 2) + std::pow(end.y - far.y, 2));
        double angle = std::acos((b*b + c*c - a*a) / (2*b*c)) * 57.29;

        // On ne compte que si l'angle < 90° et le creux est assez profond
        if (angle <= 90 && defect[3] / 256.0f > 30) {
            fingers++;
        }
    }
    return fingers;
}

QString MainWindow::detectGesture(const cv::Mat &frame) {
    int cx = frame.cols / 2; int cy = frame.rows / 2;
    int w = frame.cols / 3;  int h = frame.rows / 2;
    cv::Rect zone(cx - w/2, cy - h/2, w, h);

    cv::Mat handRegion = frame(zone);
    int f = countFingers(handRegion);

    if (f == -1) return "aucun";
    if (f == 0)  return "poing";
    if (f == 1)  return "un_doigt";
    if (f == 2)  return "deux_doigts";
    if (f == 3)  return "trois_doigts";
    return "main_ouverte";
}

// --- TRAITEMENT ET AFFICHAGE ---
void MainWindow::processFrame(const QVideoFrame &frame) {
    QImage img = frame.toImage();
    if (img.isNull()) return;

    cv::Mat mat = QImageToMat(img);
    if (mat.empty()) return;

    int cx = mat.cols / 2; int cy = mat.rows / 2;
    int w = mat.cols / 3;  int h = mat.rows / 2;
    cv::rectangle(mat, cv::Point(cx-w/2, cy-h/2), cv::Point(cx+w/2, cy+h/2), cv::Scalar(0,255,0), 2);

    QString gestureKey = detectGesture(mat);

    if (gestureKey != "aucun") {
        // 1. Récupération des sélections (Nettoyage des espaces avec trimmed)
        QString contexteActuel = ui->contexteCombo->currentText().trimmed();
        QString langueActuelle = ui->langueCombo->currentText().trimmed();

        QSqlQuery query;
        // 2. Requête filtrée sur 3 critères : Geste + Contexte + Langue
        query.prepare("SELECT word, color FROM gestures WHERE gesture = :g AND contexte = :c AND langue = :l");
        query.bindValue(":g", gestureKey);
        query.bindValue(":c", contexteActuel);
        query.bindValue(":l", langueActuelle);

        if (query.exec() && query.next()) {
            QString translation = query.value(0).toString();
            QString color = query.value(1).toString();

            ui->gestureLabel->setText(QString("Geste : %1 | %2").arg(gestureKey).arg(langueActuelle));
            ui->translationLabel->setText(translation);
            ui->translationLabel->setStyleSheet(QString("background-color: %1; color: white; border-radius: 10px; padding: 5px; font-weight: bold;").arg(color));
        } else {
            ui->gestureLabel->setText("Geste : " + gestureKey);
            ui->translationLabel->setText("Non défini (" + langueActuelle + ")");
            ui->translationLabel->setStyleSheet("color: red; font-weight: bold;");
        }
    } else {
        ui->gestureLabel->setText("Placez la main ici");
        ui->translationLabel->setText("—");
        ui->translationLabel->setStyleSheet("background-color: transparent; color: gray;");
    }

    ui->cameraLabel->setPixmap(QPixmap::fromImage(MatToQImage(mat)));
}
void MainWindow::on_startButton_clicked() { camera->start(); ui->startButton->setEnabled(false); ui->stopButton->setEnabled(true); }
void MainWindow::on_stopButton_clicked() { camera->stop(); ui->startButton->setEnabled(true); ui->stopButton->setEnabled(false); }



