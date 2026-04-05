#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMediaDevices>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("Traducteur de Gestes");

    videoWidget = new QVideoWidget(this);
    videoWidget->setGeometry(ui->cameraLabel->geometry());
    videoWidget->show();
    ui->cameraLabel->hide();

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
            videoWidget->videoSink(), &QVideoSink::setVideoFrame);

    connect(videoSink, &QVideoSink::videoFrameChanged,
            this, &MainWindow::processFrame);

    camera->start();
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->gestureLabel->setText("Geste : aucun détecté");
    ui->translationLabel->setText("Traduction : —");
}

MainWindow::~MainWindow()
{
    camera->stop();
    delete ui;
}

// Vérifie si un pixel est couleur peau
bool isSkinPixel(int r, int g, int b)
{
    return (r > 95 && g > 40 && b > 20 &&
            r > g && r > b &&
            (r - g) > 15 &&
            r < 250);
}

// Compte les pixels peau dans l'image
int MainWindow::countSkinPixels(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); y += 4) {
        for (int x = 0; x < image.width(); x += 4) {
            QColor color = image.pixelColor(x, y);
            if (isSkinPixel(color.red(), color.green(), color.blue())) {
                count++;
            }
        }
    }
    return count;
}

// Analyse l'image et retourne le geste détecté
QString MainWindow::detectGesture(const QImage &image)
{
    int skinPixels = countSkinPixels(image);

    // Pas de main détectée
    if (skinPixels < 150) {
        return "aucun";
    }

    // On analyse la forme de la main
    // On divise l'image en 3 zones : haut, milieu, bas
    int hauteur = image.height();
    int largeur = image.width();

    // Zone haute (doigts)
    int pixelsHaut = 0;
    for (int y = 0; y < hauteur / 3; y += 2) {
        for (int x = 0; x < largeur; x += 2) {
            QColor c = image.pixelColor(x, y);
            if (isSkinPixel(c.red(), c.green(), c.blue()))
                pixelsHaut++;
        }
    }

    // Zone basse (paume)
    int pixelsBas = 0;
    for (int y = (hauteur * 2) / 3; y < hauteur; y += 2) {
        for (int x = 0; x < largeur; x += 2) {
            QColor c = image.pixelColor(x, y);
            if (isSkinPixel(c.red(), c.green(), c.blue()))
                pixelsBas++;
        }
    }

    // Analyse des zones pour deviner le geste
    float ratio = (pixelsBas > 0) ? (float)pixelsHaut / pixelsBas : 0;

    if (skinPixels > 400) {
        return "main_ouverte";   // beaucoup de peau → main ouverte
    } else if (ratio < 0.3) {
        return "poing";          // peu de pixels en haut → poing fermé
    } else {
        return "un_doigt";       // pixels concentrés en haut → doigt levé
    }
}
// Appelée pour chaque image de la caméra
void MainWindow::processFrame(const QVideoFrame &frame)
{
    currentFrame = frame.toImage();
    if (currentFrame.isNull()) return;

    QImage smallImage = currentFrame.scaled(160, 120);
    QString gesture = detectGesture(smallImage);

    // Affiche le geste et sa traduction
    if (gesture == "main_ouverte") {
        ui->gestureLabel->setText("Geste : Main ouverte ✋");
        ui->translationLabel->setText("Traduction : Bonjour !");
    } else if (gesture == "poing") {
        ui->gestureLabel->setText("Geste : Poing ✊");
        ui->translationLabel->setText("Traduction : Merci !");
    } else if (gesture == "un_doigt") {
        ui->gestureLabel->setText("Geste : Un doigt ☝️");
        ui->translationLabel->setText("Traduction : Oui !");
    } else {
        ui->gestureLabel->setText("Geste : Aucun détecté");
        ui->translationLabel->setText("Traduction : —");
    }
}

void MainWindow::on_startButton_clicked()
{
    camera->start();
    videoWidget->show();
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(true);
    ui->gestureLabel->setText("Geste : en attente...");
}

void MainWindow::on_stopButton_clicked()
{
    camera->stop();
    ui->startButton->setEnabled(true);
    ui->stopButton->setEnabled(false);
    ui->gestureLabel->setText("Geste : arrêté");
}