/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "camera.h"
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
#include "ui_camera_mobile.h"
#else
#include "ui_camera.h"
#endif
#include "videosettings.h"
#include "imagesettings.h"
#include "metadatadialog.h"

#include <QMediaRecorder>
#include <QVideoWidget>
#include <QCameraDevice>
#include <QMediaMetaData>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioInput>

#include <QMessageBox>
#include <QPalette>
#include <QImage>

#include <QtWidgets>
#include <QMediaDevices>
#include <QMediaFormat>

#include <QDebug>

#include "guesser.h"

using namespace std;

Camera::Camera()
    : ui(new Ui::Camera)
{
    ui->setupUi(this);

    m_audioInput.reset(new QAudioInput);
    m_captureSession.setAudioInput(m_audioInput.get());

    //Camera devices:

    videoDevicesGroup = new QActionGroup(this);
    videoDevicesGroup->setExclusive(true);
    updateCameras();
    connect(&m_devices, &QMediaDevices::videoInputsChanged, this, &Camera::updateCameras);

    connect(videoDevicesGroup, &QActionGroup::triggered, this, &Camera::updateCameraDevice);
    connect(ui->captureWidget, &QTabWidget::currentChanged, this, &Camera::updateCaptureMode);
    this->updateCaptureMode();

    connect(ui->numberOfCards, &QSpinBox::valueChanged, this, &Camera::updateNumberOfCards);
    connect(ui->edgePosition, &QSpinBox::valueChanged, this, &Camera::updateEdgePosition);

    //connect(ui->metaDataButton, &QPushButton::clicked, this, &Camera::showMetaDataDialog);

    setCamera(QMediaDevices::defaultVideoInput());

    log = new QString("");
}

void Camera::setCamera(const QCameraDevice &cameraDevice)
{
    m_camera.reset(new QCamera(cameraDevice));
    m_captureSession.setCamera(m_camera.data());

    connect(m_camera.data(), &QCamera::activeChanged, this, &Camera::updateCameraActive);
    connect(m_camera.data(), &QCamera::errorOccurred, this, &Camera::displayCameraError);

    if (!m_mediaRecorder) {
        m_mediaRecorder.reset(new QMediaRecorder);
        m_captureSession.setRecorder(m_mediaRecorder.data());
        connect(m_mediaRecorder.data(), &QMediaRecorder::recorderStateChanged, this, &Camera::updateRecorderState);
    }

    m_imageCapture = new QImageCapture;
    m_captureSession.setImageCapture(m_imageCapture);

    connect(m_mediaRecorder.data(), &QMediaRecorder::durationChanged, this, &Camera::updateRecordTime);
    connect(m_mediaRecorder.data(), &QMediaRecorder::errorChanged, this, &Camera::displayRecorderError);

    connect(ui->exposureCompensation, &QSpinBox::valueChanged, this, &Camera::setExposureCompensation);

    m_captureSession.setVideoOutput(ui->viewfinder);

    updateCameraActive(m_camera->isActive());
    updateRecorderState(m_mediaRecorder->recorderState());

    connect(m_imageCapture, &QImageCapture::readyForCaptureChanged, this, &Camera::readyForCapture);
    connect(m_imageCapture, &QImageCapture::imageCaptured, this, &Camera::processCapturedImage);
    connect(m_imageCapture, &QImageCapture::imageSaved, this, &Camera::imageSaved);
    connect(m_imageCapture, &QImageCapture::errorOccurred, this, &Camera::displayCaptureError);
    readyForCapture(m_imageCapture->isReadyForCapture());

    updateCaptureMode();

    if (m_camera->cameraFormat().isNull()) {
        // Setting default settings.
        // The biggest resolution and the max framerate
        auto formats = cameraDevice.videoFormats();
        if (!formats.isEmpty()) {
            auto defaultFormat = formats.first();

            for (const auto &format : formats) {

                bool isFormatBigger = format.resolution().width() > defaultFormat.resolution().width()
                        && format.resolution().height() > defaultFormat.resolution().height();

                defaultFormat = isFormatBigger ? format : defaultFormat;
            }

            m_camera->setCameraFormat(defaultFormat);
            m_mediaRecorder->setVideoFrameRate(defaultFormat.maxFrameRate());
        }
    }

    m_camera->start();
}

void Camera::keyPressEvent(QKeyEvent * event)
{
    if (event->isAutoRepeat())
        return;

    switch (event->key()) {
    case Qt::Key_CameraFocus:
        displayViewfinder();
        event->accept();
        break;
    case Qt::Key_Camera:
        if (m_doImageCapture) {
            takeImage();
        } else {
            if (m_mediaRecorder->recorderState() == QMediaRecorder::RecordingState)
                stop();
            else
                record();
        }
        event->accept();
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void Camera::keyReleaseEvent(QKeyEvent *event)
{
    QMainWindow::keyReleaseEvent(event);
}

void Camera::updateRecordTime()
{
    QString str = QString("Recorded %1 sec").arg(m_mediaRecorder->duration()/1000);
    ui->statusbar->showMessage(str);
}

bool set_dim(int x) {
    return (x == 0) || (x == 3) || (x == 6);
}

void Camera::processImage(QImage qimg) {
    Q_UNUSED(qimg);
    QImage qimg2 = lastPreview;
    QImage scaledImage = qimg2.scaled(ui->viewfinder->size(),
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);

    //ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImage));
    //ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(img));

    QDebug(log) << qimg2.format();
    QDebug(log) << qimg2.height() << ", " << qimg2.width();

    int arg = ui->numberOfCards->value();
    QDebug(log) << "Attempting to find " << arg << "cards";
    cv::Mat img = cv::Mat(qimg2.height(), qimg2.width(), CV_8UC4,
                          const_cast<uchar*>(qimg2.bits()),
                          qimg2.bytesPerLine()).clone();
    cv::cvtColor(img, img, cv::COLOR_RGBA2BGR);
    cv::cvtColor(img, img, cv::COLOR_RGB2BGR);

    int rows = img.rows;
    int bottomEdge = ui->edgePosition->value();
    img.pop_back((bottomEdge*rows)/100);

    cv::Mat img_dec;
    img.copyTo(img_dec);
    decorate(arg, img_dec, m_threshold);
    cv::cvtColor(img_dec, img_dec, cv::COLOR_BGR2RGB);
    QImage qimg_dec = QImage(img_dec.data, img_dec.cols, img_dec.rows,
                             img_dec.step, QImage::Format_RGB888);
    QImage scaledImageDec = qimg_dec.scaled(ui->viewfinder->size(),
                                    Qt::KeepAspectRatio,
                                    Qt::SmoothTransformation);
    ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImageDec));

    Card cards[arg];
    try {
        QDebug(log) << m_threshold;
        //decorate(arg, img_dec, m_threshold);
//        cv::imwrite(pwd().toStdString() + "/image_dec.jpg", img_dec);
        find_cards(arg, img, cards, m_threshold);
    } catch (cv::Exception e) {
        QDebug(log) << "Caught opencv error: " << e.msg.c_str();
        //cout << "Error: " << e.msg << endl;
        //ui->statusbar->showMessage(tr("Error: \"%1\"").arg(e.msg.c_str()));
        ui->statusbar->showMessage("Adjust \"Image threshold\"");
        return;
    } catch (const char* e) {
        QDebug(log) << "Caught other error: " << e;
        //cout << e << endl;
        //ui->statusbar->showMessage(tr("Error: \"%1\"").arg(e));
        ui->statusbar->showMessage("Adjust \"Image threshold\"");
        return;
    }

    /*cv::Mat data_dec;
    cv::cvtColor(img_dec, data_dec, cv::COLOR_BGR2RGB);*/


    QDebug(log) << "Updated image!";

    //vector <Card> m_cards = vector(cards, cards + arg);

    bool found_set = false;
    for (int i = 0; i < arg - 2; i++) {
        for (int j = i + 1; j < arg - 1; j++) {
            for (int k = j + 1; k < arg; k++) {
                int shape = cards[i].shape + cards[j].shape + cards[k].shape;
                int count = cards[i].count + cards[j].count + cards[k].count;
                int color = cards[i].color + cards[j].color + cards[k].color;
                int fill = cards[i].fill + cards[j].fill + cards[k].fill;

                if (set_dim(shape) && set_dim(count) && set_dim(color) && set_dim(fill)) {
                    ui->statusbar->showMessage(tr("There is a set!"));
                    QDebug(log) << "Set at" << i << ", " << j << ", " << k;
                    QDebug(log) << "Card " << i << ": " << cards[i].shape <<
                                cards[i].count << cards[i].color <<
                                cards[i].fill;
                    QDebug(log) << "Card " << j << ": " << cards[j].shape <<
                                cards[j].count << cards[j].color <<
                                cards[j].fill;
                    QDebug(log) << "Card " << k << ": " << cards[k].shape <<
                                cards[k].count << cards[k].color <<
                                cards[k].fill;
                    found_set = true;
                    img.copyTo(img_dec);
                    decorate(arg, img_dec, m_threshold, i, j, k);
                    cv::cvtColor(img_dec, img_dec, cv::COLOR_BGR2RGB);
                    QImage qimg_dec = QImage(img_dec.data, img_dec.cols, img_dec.rows,
                                             img_dec.step, QImage::Format_RGB888);
                    QImage scaledImageDec = qimg_dec.scaled(ui->viewfinder->size(),
                                                    Qt::KeepAspectRatio,
                                                    Qt::SmoothTransformation);
                    ui->lastImagePreviewLabel->setPixmap(QPixmap::fromImage(scaledImageDec));
                    goto afterLoops;
                }
            }
        }
    }
    afterLoops:

    if (!found_set) {
        ui->statusbar->showMessage(tr("There is no set :("));
    }
}

void Camera::processCapturedImage(int requestId, const QImage& qimg)
{
    Q_UNUSED(requestId);
    ui->captureWidget->setCurrentIndex(1);
    lastPreview = qimg.copy();
    imageTaken = true;
    processImage(lastPreview);
    // Display captured image for 4 seconds.
    displayCapturedImage();
    QTimer::singleShot(100000, this, &Camera::displayViewfinder);


}

void Camera::configureCaptureSettings()
{
    if (m_doImageCapture)
        configureImageSettings();
    else
        configureVideoSettings();
}

void Camera::configureVideoSettings()
{
    VideoSettings settingsDialog(m_mediaRecorder.data());
    settingsDialog.setWindowFlags(settingsDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (settingsDialog.exec())
        settingsDialog.applySettings();
}

void Camera::configureImageSettings()
{
    ImageSettings settingsDialog(m_imageCapture);
    settingsDialog.setWindowFlags(settingsDialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (settingsDialog.exec()) {
        settingsDialog.applyImageSettings();
    }
}

void Camera::record()
{
    m_mediaRecorder->record();
    updateRecordTime();
}

void Camera::pause()
{
    m_mediaRecorder->pause();
}

void Camera::stop()
{
    m_mediaRecorder->stop();
}

void Camera::setMuted(bool muted)
{
    m_captureSession.audioInput()->setMuted(muted);
}

void Camera::takeImage()
{
    m_isCapturingImage = true;
    m_imageCapture->captureToFile();
}

void Camera::displayCaptureError(int id, const QImageCapture::Error error, const QString &errorString)
{
    Q_UNUSED(id);
    Q_UNUSED(error);
    QMessageBox::warning(this, tr("Image Capture Error"), errorString);
    m_isCapturingImage = false;
}

void Camera::startCamera()
{
    m_camera->start();
}

void Camera::stopCamera()
{
    m_camera->stop();
}

void Camera::updateNumberOfCards() {
    if (imageTaken) processImage(lastPreview);
}

void Camera::updateEdgePosition() {
    /*ui->viewfinder->setProperty("margin-bottom", QString::number(ui->edgePosition->value()) + "%");

    QEvent event(QEvent::StyleChange);
    QApplication::sendEvent(ui->edgePosition, &event);
    ui->edgePosition->update();
    ui->edgePosition->updateGeometry();*/

    if (imageTaken) processImage(lastPreview);
}

void Camera::updateCaptureMode()
{
    /*int tabIndex = ui->captureWidget->currentIndex();
    m_doImageCapture = (tabIndex == 0);*/
    m_doImageCapture = true;
    if (ui->captureWidget->currentIndex() == 2) {
        ui->log->setText(*log);
        qDebug() << log;
    }
    if (ui->captureWidget->currentIndex() == 0) {
        displayViewfinder();
    } else {
        displayCapturedImage();
        if (imageTaken) processImage(lastPreview);
    }
}

void Camera::updateCameraActive(bool active)
{
    if (active) {
        ui->actionStartCamera->setEnabled(false);
        ui->actionStopCamera->setEnabled(true);
        //ui->captureWidget->setEnabled(true);
        ui->actionSettings->setEnabled(true);
    } else {
        ui->actionStartCamera->setEnabled(true);
        ui->actionStopCamera->setEnabled(false);
        //ui->captureWidget->setEnabled(false);
        ui->actionSettings->setEnabled(false);
    }
}

void Camera::updateRecorderState(QMediaRecorder::RecorderState state)
{
    switch (state) {
    case QMediaRecorder::StoppedState:
        /*ui->recordButton->setEnabled(true);
        ui->pauseButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        ui->metaDataButton->setEnabled(true);*/
        break;
    case QMediaRecorder::PausedState:
        /*ui->recordButton->setEnabled(true);
        ui->pauseButton->setEnabled(false);
        ui->stopButton->setEnabled(true);
        ui->metaDataButton->setEnabled(false);*/
        break;
    case QMediaRecorder::RecordingState:
        /*ui->recordButton->setEnabled(false);
        ui->pauseButton->setEnabled(true);
        ui->stopButton->setEnabled(true);
        ui->metaDataButton->setEnabled(false);*/
        break;
    }
}

void Camera::setExposureCompensation(int index)
{
    //m_camera->setExposureCompensation(index*0.5);
    m_threshold = index;
    if (imageTaken) {
        processImage(lastPreview);
    }
    ui->exposureCompensation->clearFocus();
}

void Camera::displayRecorderError()
{
    if (m_mediaRecorder->error() != QMediaRecorder::NoError)
        QMessageBox::warning(this, tr("Capture Error"), m_mediaRecorder->errorString());
}

void Camera::displayCameraError()
{
    if (m_camera->error() != QCamera::NoError)
        QMessageBox::warning(this, tr("Camera Error"), m_camera->errorString());
}

void Camera::updateCameraDevice(QAction *action)
{
    setCamera(qvariant_cast<QCameraDevice>(action->data()));
}

void Camera::displayViewfinder()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void Camera::displayCapturedImage()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void Camera::readyForCapture(bool ready)
{
    ui->takeImageButton->setEnabled(ready);
}

void Camera::imageSaved(int id, const QString &fileName)
{
    Q_UNUSED(id);
    Q_UNUSED(fileName);
    //ui->statusbar->showMessage(tr("Captured \"%1\"").arg(QDir::toNativeSeparators(fileName)));

    m_isCapturingImage = false;
    if (m_applicationExiting)
        close();
}

void Camera::closeEvent(QCloseEvent *event)
{
    if (m_isCapturingImage) {
        setEnabled(false);
        m_applicationExiting = true;
        event->ignore();
    } else {
        event->accept();
    }
}

void Camera::updateCameras()
{
    ui->menuDevices->clear();
    const QList<QCameraDevice> availableCameras = QMediaDevices::videoInputs();
    for (const QCameraDevice &cameraDevice : availableCameras) {
        QAction *videoDeviceAction = new QAction(cameraDevice.description(), videoDevicesGroup);
        videoDeviceAction->setCheckable(true);
        videoDeviceAction->setData(QVariant::fromValue(cameraDevice));
        if (cameraDevice == QMediaDevices::defaultVideoInput())
            videoDeviceAction->setChecked(true);

        ui->menuDevices->addAction(videoDeviceAction);
    }
}

void Camera::showMetaDataDialog()
{
    if (!m_metaDataDialog)
        m_metaDataDialog = new MetaDataDialog(this);
    m_metaDataDialog->setAttribute(Qt::WA_DeleteOnClose, false);
    if (m_metaDataDialog->exec() == QDialog::Accepted)
        saveMetaData();
}

void Camera::saveMetaData()
{
    QMediaMetaData data;
    for (int i = 0; i < QMediaMetaData::NumMetaData; i++) {
        QString val = m_metaDataDialog->m_metaDataFields[i]->text();
        if (!val.isEmpty()) {
            auto key = static_cast<QMediaMetaData::Key>(i);
            if (i == QMediaMetaData::CoverArtImage) {
                QImage coverArt(val);
                data.insert(key, coverArt);
            }
            else if (i == QMediaMetaData::ThumbnailImage) {
                QImage thumbnail(val);
                data.insert(key, thumbnail);
            }
            else if (i == QMediaMetaData::Date) {
                QDateTime date = QDateTime::fromString(val);
                data.insert(key, date);
            }
            else {
                data.insert(key, val);
            }
        }
    }
    m_mediaRecorder->setMetaData(data);
}

