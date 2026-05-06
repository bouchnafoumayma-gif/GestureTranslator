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
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/gestures.db");

    if (!db.open()) {
        qDebug() << "Erreur : Impossible d'ouvrir la base de données" << db.lastError().text();
    } else {
        qDebug() << "Connexion réussie à la base gestures.";
    }

    // 2. Configuration de l'Interface
    setWindowTitle("GestureTranslator - ENSAH");
    ui->cameraLabel->setScaledContents(true);

    // Initialisation des listes déroulantes
    if(ui->contexteCombo->count() == 0) {
        ui->contexteCombo->addItems({"General", "Sante", "Restaurant"});
    }
    if(ui->langueCombo->count() == 0) {
        ui->langueCombo->addItems({"FR", "EN", "AR"});
    }

    // Initialisation du mode Urgence
    isUrgencyMode = false;
    ui->sosButton->setStyleSheet("background-color: #8B0000; color: white; font-weight: bold; border-radius: 5px;");

    // 3. Configuration Caméra
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

// --- Fonctions de conversion ---
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

// --- ALGORITHME (Loi des Cosinus) ---
int MainWindow::countFingers(const cv::Mat &handRegion) {
    cv::Mat gray, binary;
    cv::cvtColor(handRegion, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY + cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return -1;

    // Trouver le plus grand contour
    int maxIdx = 0;
    double maxArea = 0;
    for (int i = 0; i < (int)contours.size(); i++) {
        double area = cv::contourArea(contours[i]);
        if (area > maxArea) { maxArea = area; maxIdx = i; }
    }

    // --- SÉCURITÉ 1 : Taille minimum ---
    if (maxArea < 5000) return -1;

    // --- SÉCURITÉ 2 : Simplification du contour ---
    // Cela évite les "auto-intersections" mentionnées dans ton erreur
    std::vector<cv::Point> approxContour;
    double peri = cv::arcLength(contours[maxIdx], true);
    cv::approxPolyDP(contours[maxIdx], approxContour, 0.001 * peri, true);

    std::vector<int> hullIdx;
    cv::convexHull(approxContour, hullIdx, false);

    // --- SÉCURITÉ 3 : Vérification de la forme ---
    if (hullIdx.size() < 3) return 0;

    try {
        std::vector<cv::Vec4i> defects;
        cv::convexityDefects(approxContour, hullIdx, defects);

        int fingers = 0;
        for (const cv::Vec4i &defect : defects) {
            float depth = defect[3] / 256.0f;
            if (depth > 20) { // Filtre les petits creux (bruit)
                cv::Point start = approxContour[defect[0]];
                cv::Point end   = approxContour[defect[1]];
                cv::Point far   = approxContour[defect[2]];

                double a = std::sqrt(std::pow(end.x - start.x, 2) + std::pow(end.y - start.y, 2));
                double b = std::sqrt(std::pow(far.x - start.x, 2) + std::pow(far.y - start.y, 2));
                double c = std::sqrt(std::pow(end.x - far.x, 2) + std::pow(end.y - far.y, 2));

                // Loi des cosinus
                double angle = std::acos((b*b + c*c - a*a) / (2*b*c)) * 57.29;

                if (angle <= 90) {
                    fingers++;
                }
            }
        }
        return fingers;
    } catch (cv::Exception& e) {
        // Si OpenCV détecte quand même une erreur, on ne plante pas, on ignore juste la frame
        qDebug() << "Erreur OpenCV évitée :" << e.what();
        return -1;
    }
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
        // --- LOGIQUE DE PRIORITÉ SOS ---
        QString contexteActuel;
        if (isUrgencyMode) {
            contexteActuel = "Urgence"; // Force le contexte
        } else {
            contexteActuel = ui->contexteCombo->currentText().trimmed();
        }

        QString langueActuelle = ui->langueCombo->currentText().trimmed();

        QSqlQuery query;
        query.prepare("SELECT word, color FROM gestures WHERE gesture = :g AND contexte = :c AND langue = :l");
        query.bindValue(":g", gestureKey);
        query.bindValue(":c", contexteActuel);
        query.bindValue(":l", langueActuelle);

        if (query.exec() && query.next()) {
            QString translation = query.value(0).toString();
            QString color = query.value(1).toString();

            ui->gestureLabel->setText(QString("Geste : %1 | %2").arg(gestureKey).arg(langueActuelle));
            ui->translationLabel->setText(translation);

            // Style spécial si Urgence
            QString style = QString("background-color: %1; color: white; border-radius: 10px; padding: 5px; font-weight: bold;").arg(color);
            if(isUrgencyMode) style += "border: 3px solid yellow; font-size: 20px;";

            ui->translationLabel->setStyleSheet(style);
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

// --- SLOTS DES BOUTONS ---
void MainWindow::on_sosButton_clicked() {
    isUrgencyMode = !isUrgencyMode;

    if (isUrgencyMode) {
        ui->sosButton->setStyleSheet("background-color: #FF0000; color: white; font-weight: bold; border: 4px solid #FFFF00; border-radius: 5px;");
        ui->sosButton->setText("!!! SOS ACTIF !!!");
    } else {
        ui->sosButton->setStyleSheet("background-color: #8B0000; color: white; font-weight: bold; border-radius: 5px;");
        ui->sosButton->setText("URGENCE / SOS");
    }
}

void MainWindow::on_startButton_clicked() {
    camera->start();
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
}

void MainWindow::on_stopButton_clicked() {
    camera->stop();
    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);
}