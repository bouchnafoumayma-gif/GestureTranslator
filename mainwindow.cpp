#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMediaDevices>
#include <QPixmap>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <QTimer>

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
    ui->imageModele->setScaledContents(true);

    // Synchronisation avec les valeurs de la base (image_e3490e.png)
    if(ui->contexteCombo->count() == 0) {
        ui->contexteCombo->addItems({"General", "Sante", "Restaurant"});
    }
    if(ui->langueCombo->count() == 0) {
        ui->langueCombo->addItems({"FR", "EN", "AR"});
    }

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

    ui->apprentissageFrame->setVisible(false);
    isApprentissageMode = false;
    progressCount = 0;

    timerApprentissage = new QTimer(this);
    connect(timerApprentissage, &QTimer::timeout, this, &MainWindow::validerGeste);
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

    if (maxArea < 5000) return -1;

    std::vector<cv::Point> approxContour;
    double peri = cv::arcLength(contours[maxIdx], true);
    cv::approxPolyDP(contours[maxIdx], approxContour, 0.001 * peri, true);

    std::vector<int> hullIdx;
    cv::convexHull(approxContour, hullIdx, false);

    if (hullIdx.size() < 3) return 0;

    try {
        std::vector<cv::Vec4i> defects;
        cv::convexityDefects(approxContour, hullIdx, defects);

        int fingers = 0;
        for (const cv::Vec4i &defect : defects) {
            float depth = defect[3] / 256.0f;
            if (depth > 20) {
                cv::Point start = approxContour[defect[0]];
                cv::Point end   = approxContour[defect[1]];
                cv::Point far   = approxContour[defect[2]];

                double a = std::sqrt(std::pow(end.x - start.x, 2) + std::pow(end.y - start.y, 2));
                double b = std::sqrt(std::pow(far.x - start.x, 2) + std::pow(far.y - start.y, 2));
                double c = std::sqrt(std::pow(end.x - far.x, 2) + std::pow(end.y - far.y, 2));

                double angle = std::acos((b*b + c*c - a*a) / (2*b*c)) * 57.29;
                if (angle <= 90) fingers++;
            }
        }
        return fingers;
    } catch (cv::Exception& e) {
        return -1;
    }
}

QString MainWindow::detectGesture(const cv::Mat &frame) {
    int cx = frame.cols / 2; int cy = frame.rows / 2;
    int w = frame.cols / 3;  int h = frame.rows / 2;
    cv::Rect zone(cx - w/2, cy - h/2, w, h);
    cv::Mat handRegion = frame(zone);

    int f = countFingers(handRegion);

    // CORRECTION : Harmonisation avec les noms de la table SQL (pluriels)
    if (f == -1) return "aucun";
    if (f == 0)  return "poing";
    if (f == 1)  return "un_doigt";
    if (f == 2)  return "deux_doigts";
    if (f == 3)  return "trois_doigts"; // Ajout du 's' pour correspondre à image_e3490e.png
    return "main_ouverte";
}

void MainWindow::processFrame(const QVideoFrame &frame) {
    QImage img = frame.toImage();
    if (img.isNull()) return;

    cv::Mat mat = QImageToMat(img);
    if (mat.empty()) return;

    int cx = mat.cols / 2; int cy = mat.rows / 2;
    int w = mat.cols / 3;  int h = mat.rows / 2;
    cv::rectangle(mat, cv::Point(cx-w/2, cy-h/2), cv::Point(cx+w/2, cy+h/2), cv::Scalar(0,255,0), 2);

    QString gestureKey = detectGesture(mat);

    if (isApprentissageMode) {
        ui->gestureLabel->setText(QString("Mode Apprentissage | Cible : %1").arg(gesteCible));
        if (gestureKey == gesteCible) {
            if (!timerApprentissage->isActive()) timerApprentissage->start(1000);
            ui->translationLabel->setText(QString("Maintenez encore... %1/3s").arg(progressCount));
        } else {
            timerApprentissage->stop();
            progressCount = 0;
            ui->translationLabel->setText(gestureKey == "aucun" ? "Imitez l'image" : "Mauvais geste");
        }
    }
    else {
        if (gestureKey != "aucun") {
            QString contexteActuel = isUrgencyMode ? "Urgence" : ui->contexteCombo->currentText().trimmed();
            QString langueActuelle = ui->langueCombo->currentText().trimmed();

            QSqlQuery query;
            // CORRECTION : La colonne de langue dans vos images s'appelle 'translation'
            query.prepare("SELECT word, color FROM gestes WHERE gesture = :g AND contexte = :c AND translation = :l");
            query.bindValue(":g", gestureKey);
            query.bindValue(":c", contexteActuel);
            query.bindValue(":l", langueActuelle);

            if (query.exec() && query.next()) {
                QString translationText = query.value(0).toString();
                QString colorName = query.value(1).toString();

                ui->gestureLabel->setText(QString("Geste : %1 (%2)").arg(gestureKey).arg(langueActuelle));
                ui->translationLabel->setText(translationText);
                ui->translationLabel->setStyleSheet(QString("background-color: %1; color: white; border-radius: 10px; font-weight: bold;").arg(colorName));
            } else {
                ui->gestureLabel->setText("Geste : " + gestureKey);
                ui->translationLabel->setText("Non défini (" + langueActuelle + ")");
                ui->translationLabel->setStyleSheet("color: red; font-weight: bold;");
                // Log pour debug
                qDebug() << "Non trouvé:" << gestureKey << contexteActuel << langueActuelle;
            }
        } else {
            ui->gestureLabel->setText("Placez la main dans le cadre");
            ui->translationLabel->setText("—");
            ui->translationLabel->setStyleSheet("color: gray;");
        }
    }
    ui->cameraLabel->setPixmap(QPixmap::fromImage(MatToQImage(mat)));
}

// ... Reste des slots (SOS, start, stop, apprentissage) ...

// --- SLOTS ---
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

void MainWindow::on_apprentissageToggle_clicked() {
    isApprentissageMode = !isApprentissageMode;
    ui->apprentissageFrame->setVisible(isApprentissageMode);
    ui->contexteCombo->setVisible(!isApprentissageMode);
    ui->langueCombo->setVisible(!isApprentissageMode);
    ui->sosButton->setVisible(!isApprentissageMode);

    if (isApprentissageMode) {
        ui->apprentissageToggle->setText("Retour à la Traduction");
        gesteCible = "poing";
        ui->consigneLabel->setText("Imitez le poing pour dire 'J'ai mal'");
        // Chemin mis à jour selon ton fichier .qrc (prefix /images + folder images/)
        ui->imageModele->setPixmap(QPixmap(":/images/images/poing.jpg"));
    } else {
        ui->apprentissageToggle->setText("Mode Apprentissage");
        timerApprentissage->stop();
        progressCount = 0;
    }
}

void MainWindow::validerGeste() {
    progressCount++;
    if (progressCount >= 3) {
        timerApprentissage->stop();
        ui->translationLabel->setText("BRAVO ! Geste maîtrisé.");
        ui->translationLabel->setStyleSheet("background-color: green; color: white; border-radius: 10px; font-weight: bold;");
        progressCount = 0;

        // Logique pour passer au geste suivant après 2 secondes
        QTimer::singleShot(2000, this, [this](){
            if (gesteCible == "poing") {
                gesteCible = "un_doigt";
                ui->consigneLabel->setText("Imitez l'index pour dire 'Santé'");
                ui->imageModele->setPixmap(QPixmap(":/images/images/un_doigt.webp"));
            } else if (gesteCible == "un_doigt") {
                gesteCible = "deux_doigts";
                ui->consigneLabel->setText("Faites le signe V pour 'Merci'");
                ui->imageModele->setPixmap(QPixmap(":/images/images/deux_doigts.webp"));
            }
        });
    }
}