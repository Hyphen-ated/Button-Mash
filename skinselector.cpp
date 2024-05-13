#include <QDomDocument>
#include <QDebug>
#include <QFileDialog>

#include "skinselector.h"
#include "ui_skinselector.h"
#include "inputdisplay.h"
#include "skinparser.h"
#include "sqpath.h"

#include <QDir>
#include <QStandardItemModel>

QSettings* globalSetting;

SkinSelector::SkinSelector(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SkinSelector)
{
    ui->setupUi(this);
    listModel = new QStandardItemModel();
    ui->skinListView->setModel(listModel);
    subSkinModel = new QStandardItemModel();
    ui->subSkinListView->setModel(subSkinModel);
    pianoModel = new QStandardItemModel();
    ui->pianoSkinListView->setModel(pianoModel);
#ifdef Q_OS_WINDOWS
    globalSetting = new QSettings("ButtonMash", QSettings::IniFormat);
#else
    globalSetting = new QSettings("ButtonMash");
#endif
    //globalSetting->clear();
    if (globalSetting->contains("skinFolder"))
    {
        setSkinPath(globalSetting->value("skinFolder").toString());
    } else {
        setSkinPath(SQPath::softwareDatasPath() + "/Skins");
        if (listModel->item(0) != nullptr)
            on_skinListView_clicked(listModel->item(0)->index());
    }
    timer.setInterval(50);
    timer.start();
    display = nullptr;
    inputProvider = nullptr;
    connect(&timer, &QTimer::timeout, this, &SkinSelector::onTimerTimeout);
    inputSelector = new InputSourceSelector(this);
    ui->configHSButton->setVisible(false);
}

void    SkinSelector::setPreviewScene(const RegularSkin& skin)
{
    QGraphicsScene* scene = ui->previewGraphicView->scene();
    if (scene == nullptr)
    {
        scene = new QGraphicsScene();
        ui->previewGraphicView->setScene(scene);
    }
    scene->clear();
    QFileInfo fi(skin.file);
    QPixmap background(fi.absolutePath() + "/" + skin.background);
    scene->setSceneRect(0, 0, background.size().width(), background.size().height());
    scene->addPixmap(background);
    foreach(RegularButtonSkin but, skin.buttons)
    {
        QPixmap pix(fi.absolutePath() + "/" + but.image);
        QGraphicsPixmapItem* newPix = new QGraphicsPixmapItem(pix.scaled(but.width, but.height));
        newPix->setPos(but.x, but.y);
        newPix->setZValue(1);
        scene->addItem(newPix);
    }
    ui->previewGraphicView->fitInView(scene->itemsBoundingRect(), Qt::KeepAspectRatio);
}

void    SkinSelector::restoreLastSkin()
{
    //Skin
    for (unsigned int i = 0; i < listModel->rowCount(); i++)
    {
        const RegularSkin& sk = listModel->item(i)->data(Qt::UserRole + 2).value<RegularSkin>();
        if (sk.file == globalSetting->value("lastSkin/regularSkinPath").toString())
        {
            ui->skinListView->setCurrentIndex(listModel->item(i)->index());
            on_skinListView_clicked(listModel->item(i)->index());
            currentSkin = sk;
            if (!sk.subSkins.isEmpty())
            {
                unsigned int j = 0;
                qDebug() << "Sub skin" << globalSetting->value("lastSkin/regularSubSkin").toString();
                foreach(RegularSkin ssk, sk.subSkins)
                {
                    qDebug() << ssk.name;
                    if (ssk.name == globalSetting->value("lastSkin/regularSubSkin").toString())
                    {
                        ui->subSkinListView->setCurrentIndex(subSkinModel->item(j)->index());
                        qDebug() << "Crash is here?";
                        currentSkin = ssk;
                        break;
                    }
                    j++;
                }
            }
            qDebug() << "Set preview";
            setPreviewScene(currentSkin);

            break;
        }
    }
    //Piano Display
    qDebug() << "Piano Display" << globalSetting->value("lastSkin/pianoDisplay");
    if (!globalSetting->value("lastSkin/pianoDisplay").toBool())
        return;
    ui->pianoCheckBox->setChecked(true);
    for (unsigned int i = 0; i < pianoModel->rowCount(); i++)
    {
        const PianoSkin& sk = pianoModel->item(i)->data(Qt::UserRole + 2).value<PianoSkin>();
        if (sk.file == globalSetting->value("lastSkin/pianoSkinPath").toString())
        {
            ui->pianoSkinListView->setCurrentIndex(pianoModel->item(i)->index());
            break;
        }
    }
}

void    SkinSelector::saveSkinStarted()
{
    globalSetting->setValue("lastSkin/pianoDisplay", ui->pianoCheckBox->isChecked());
    globalSetting->setValue("lastSkin/regularSkinPath", currentSkin.file);
    if (ui->pianoSkinListView->currentIndex().isValid())
    {
        globalSetting->setValue("lastSkin/pianoSkinPath", pianoModel->itemFromIndex(
                                ui->pianoSkinListView->currentIndex())->data(Qt::UserRole + 2).value<PianoSkin>().file);
    }
    globalSetting->setValue("lastSkin/regularSubSkin", currentSkin.name);

}

void    SkinSelector::setSkinPath(QString path)
{
    listModel->clear();
    pianoModel->clear();
#ifdef SQPROJECT_INSTALLED
    addSkinPath(SQPath::softwareDatasPath() + "/Skins");
    if (path == SQPath::softwareDatasPath() + "/Skins")
        return ;
#endif
    globalSetting->setValue("skinFolder", path);
    addSkinPath(path);
}

void    SkinSelector::addSkinPath(QString path)
{
    ui->skinPathEdit->setText(path);
    QDir dir(path);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    auto list = dir.entryInfoList();
    foreach(QFileInfo fi, list)
    {
        qDebug() << fi.fileName();
        if (QFileInfo::exists(fi.absoluteFilePath() + "/skin.xml"))
        {
            qDebug() << "Found a skin file";
            RegularSkin skin = SkinParser::parseRegularSkin(fi.absoluteFilePath() + "/skin.xml");
            if (!SkinParser::xmlError.isEmpty())
                qDebug() << SkinParser::xmlError;
            //qDebug() << skin;
            QStandardItem* item = new QStandardItem(QString(tr("%1 by %2")).arg(skin.name).arg(skin.author));
            QVariant var;
            var.setValue(skin);
            item->setData(var, Qt::UserRole + 2);
            listModel->appendRow(item);
            //ui->skinListView->setCurrentIndex(listModel->item(0)->index());
        }
        if (QFileInfo::exists(fi.absoluteFilePath() + "/pianodisplay.xml"))
        {
            ui->pianoCheckBox->setEnabled(true);
            qDebug() << "Found a piano skin file";
            PianoSkin pSkin = SkinParser::parsePianoSkin(fi.absoluteFilePath() + "/pianodisplay.xml");
            //qDebug() << pSkin;
            QStandardItem* item = new QStandardItem(QString(tr("%1 by %2")).arg(pSkin.name).arg(pSkin.author));
            QVariant var;
            var.setValue(pSkin);
            item->setData(var, Qt::UserRole + 2);
            pianoModel->appendRow(item);
            //ui->pianoSkinListView->setCurrentIndex(pianoModel->item(0)->index());
        }

    }
}

SkinSelector::~SkinSelector()
{
    delete ui;
}



void SkinSelector::on_startButton_clicked()
{
    PianoSkin pSkin;
    if (ui->pianoCheckBox->isChecked())
    {
        pSkin = pianoModel->itemFromIndex(
                ui->pianoSkinListView->currentIndex())->data(Qt::UserRole + 2).value<PianoSkin>();
    }
    display = new InputDisplay(currentSkin, pSkin);
    display->setInputProvider(inputProvider);
    if (inputSelector->delai() != 0)
        display->setDelai(inputSelector->delai());
    connect(display, &InputDisplay::closed, this, &SkinSelector::onDisplayClosed);
    display->show();
    inputProvider->start();
    this->hide();
    saveSkinStarted();
  }

void SkinSelector::on_pianoCheckBox_stateChanged(int arg1)
{
    ui->pianoSkinListView->setEnabled(arg1 != 0);
}

void SkinSelector::on_skinListView_clicked(const QModelIndex &index)
{
    const RegularSkin& skin = listModel->itemFromIndex(index)->data(Qt::UserRole + 2).value<RegularSkin>();
    subSkinModel->clear();
    if (skin.subSkins.isEmpty())
        currentSkin = skin;
    else
        currentSkin = skin.subSkins.first();
    foreach(RegularSkin sk, skin.subSkins)
    {
        QStandardItem* item = new QStandardItem(sk.name);
        QVariant var;
        var.setValue(sk);
        item->setData(var, Qt::UserRole + 2);
        subSkinModel->appendRow(item);
        ui->subSkinListView->setCurrentIndex(subSkinModel->item(0)->index());
    }
    setPreviewScene(skin);
}

void SkinSelector::onTimerTimeout()
{
    static bool first = true;
    if (first)
    {
        first = false;
        timer.setInterval(1000);
        restoreLastSkin();
        inputProvider = inputSelector->getLastProvider();
        if (inputProvider != nullptr)
            ui->sourceLabel->setText(QString("<b>%1</b>").arg(inputProvider->name()));
        else
            ui->sourceLabel->setText(tr("No Source provider selected"));
        return ;
    }
    if ((display != nullptr && display->isVisible()) || inputProvider == nullptr)
        return ;
    if (inputProvider->isReady() && !currentSkin.name.isEmpty())
        ui->startButton->setEnabled(true);
    else {
        ui->startButton->setEnabled(false);
    }
    if (currentSkin.name.isEmpty())
        ui->statusLabel->setText(tr("You need to select a skin. ") + inputProvider->statusText());
    else {
        ui->statusLabel->setText(inputProvider->statusText());
    }
}

void SkinSelector::on_skinPathButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose the default folder for skins"), qApp->applicationDirPath() + "/Skins");
    if (!dir.isEmpty())
    {
        setSkinPath(dir);
        if (listModel->item(0) != nullptr)
            on_skinListView_clicked(listModel->item(0)->index());
    }
}

void SkinSelector::on_subSkinListView_clicked(const QModelIndex &index)
{
    const RegularSkin& skin = subSkinModel->itemFromIndex(index)->data(Qt::UserRole + 2).value<RegularSkin>();
    currentSkin = skin;
    setPreviewScene(skin);
}


void SkinSelector::onDisplayClosed()
{
    show();
    inputProvider->stop();
}

void SkinSelector::on_changeSourceButton_clicked()
{
    timer.stop();
    if (inputSelector->exec())
    {
        inputProvider = inputSelector->currentProvider();
        ui->sourceLabel->setText(QString("<b>%1</b>").arg(inputProvider->name()));
        timer.start();
    }
}


void SkinSelector::on_configHSButton_clicked()
{
    static bool hide = true;
    if (hide)
        ui->configFrame->hide();
    else
        ui->configFrame->show();
    hide = !hide;
}
