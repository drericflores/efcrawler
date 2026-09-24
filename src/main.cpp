#include "core/ResearchEngine.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr auto kVersion = "0.2.3";
constexpr auto kProgramDate = "September 24, 2026";

enum class Theme {
    Dark,
    Light
};

QString downloadDirectory()
{
    QString root =
        QStandardPaths::writableLocation(
            QStandardPaths::DownloadLocation);

    if (root.isEmpty()) {
        root =
            QDir::homePath() +
            QStringLiteral("/Downloads");
    }

    return root +
           QStringLiteral("/eFCrawler");
}

QString safeFileName(const QUrl& url)
{
    QString name =
        QFileInfo(url.path()).fileName();

    if (name.isEmpty()) {
        name = QStringLiteral("resource");
    }

    static const QRegularExpression unsafe(
        QStringLiteral(
            R"([^A-Za-z0-9._()\- ]+)"));

    name.replace(
        unsafe,
        QStringLiteral("_"));

    return name;
}

QString darkStyle()
{
    return QStringLiteral(R"(
        QMainWindow {
            background: #101827;
        }

        QMenuBar {
            background: #09111f;
            color: #f8fafc;
            padding: 4px;
            border-bottom: 1px solid #334155;
        }

        QMenuBar::item {
            padding: 6px 12px;
            background: transparent;
        }

        QMenuBar::item:selected {
            background: #334155;
            border-radius: 4px;
        }

        QMenu {
            background: #182235;
            color: #f8fafc;
            border: 1px solid #475569;
            padding: 5px;
        }

        QMenu::item {
            padding: 7px 30px 7px 12px;
        }

        QMenu::item:selected {
            background: #2563eb;
            color: white;
        }

        QWidget#central {
            background: #101827;
        }

        QFrame#headerCard,
        QFrame#researchCard,
        QFrame#resultsCard,
        QFrame#statusCard {
            background: #1a2639;
            border: 1px solid #3a4a63;
            border-radius: 10px;
        }

        QLabel {
            color: #e8eef7;
        }

        QLabel#brand {
            color: #ffffff;
            font-size: 28px;
            font-weight: 700;
        }

        QLabel#subtitle {
            color: #b8c7dc;
            font-size: 13px;
        }

        QLabel#sectionTitle {
            color: #ffffff;
            font-size: 14px;
            font-weight: 700;
        }

        QLabel#counter {
            color: #7dd3fc;
            font-size: 13px;
            font-weight: 700;
        }

        QLabel#provider {
            color: #e2e8f0;
            padding: 5px 9px;
            background: #0d1728;
            border: 1px solid #263751;
            border-radius: 5px;
        }

        QCheckBox {
            color: #f1f5f9;
            spacing: 7px;
            font-weight: 600;
        }

        QLineEdit {
            background: #0d1728;
            color: #ffffff;
            border: 1px solid #52647e;
            border-radius: 7px;
            padding: 10px 12px;
            selection-background-color: #2563eb;
            font-size: 14px;
        }

        QLineEdit:focus {
            border: 1px solid #60a5fa;
        }

        QPushButton {
            background: #30425f;
            color: #ffffff;
            border: 1px solid #52647e;
            border-radius: 6px;
            padding: 8px 14px;
            font-weight: 600;
        }

        QPushButton:hover {
            background: #405677;
            border-color: #7c91ad;
        }

        QPushButton:disabled {
            color: #8090a6;
            background: #1c293c;
            border-color: #34445b;
        }

        QPushButton#primary {
            background: #2563eb;
            border-color: #60a5fa;
        }

        QPushButton#primary:hover {
            background: #1d4ed8;
        }

        QPushButton#danger {
            background: #991b1b;
            border-color: #dc2626;
        }

        QTableView {
            background: #f8fafc;
            alternate-background-color: #e8eef6;
            color: #172033;
            border: 1px solid #64748b;
            border-radius: 6px;
            gridline-color: #cbd5e1;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }

        QTableView::item {
            padding: 7px;
            border-bottom: 1px solid #d5dde8;
        }

        QHeaderView::section {
            background: #263852;
            color: #ffffff;
            padding: 9px;
            border: none;
            border-right: 1px solid #52647e;
            border-bottom: 1px solid #52647e;
            font-weight: 700;
        }

        QProgressBar {
            background: #0d1728;
            color: #ffffff;
            border: 1px solid #52647e;
            border-radius: 5px;
            text-align: center;
            min-height: 20px;
        }

        QProgressBar::chunk {
            background: #2563eb;
            border-radius: 4px;
        }

        QStatusBar {
            background: #09111f;
            color: #e2e8f0;
            border-top: 1px solid #334155;
        }
    )");
}

QString lightStyle()
{
    return QStringLiteral(R"(
        QMainWindow {
            background: #e8edf4;
        }

        QMenuBar {
            background: #17365d;
            color: #ffffff;
            padding: 4px;
            border-bottom: 1px solid #102a49;
        }

        QMenuBar::item {
            padding: 6px 12px;
            background: transparent;
        }

        QMenuBar::item:selected {
            background: #2c5687;
            border-radius: 4px;
        }

        QMenu {
            background: #ffffff;
            color: #172033;
            border: 1px solid #a9b7c8;
            padding: 5px;
        }

        QMenu::item {
            padding: 7px 30px 7px 12px;
        }

        QMenu::item:selected {
            background: #2563eb;
            color: #ffffff;
        }

        QWidget#central {
            background: #e8edf4;
        }

        QFrame#headerCard,
        QFrame#researchCard,
        QFrame#resultsCard,
        QFrame#statusCard {
            background: #ffffff;
            border: 1px solid #b7c3d1;
            border-radius: 10px;
        }

        QLabel {
            color: #26374a;
        }

        QLabel#brand {
            color: #17365d;
            font-size: 28px;
            font-weight: 700;
        }

        QLabel#subtitle {
            color: #52677f;
            font-size: 13px;
        }

        QLabel#sectionTitle {
            color: #17365d;
            font-size: 14px;
            font-weight: 700;
        }

        QLabel#counter {
            color: #075985;
            font-size: 13px;
            font-weight: 700;
        }

        QLabel#provider {
            color: #17365d;
            padding: 5px 9px;
            background: #edf3f9;
            border: 1px solid #b7c3d1;
            border-radius: 5px;
        }

        QCheckBox {
            color: #26374a;
            spacing: 7px;
            font-weight: 600;
        }

        QLineEdit {
            background: #ffffff;
            color: #172033;
            border: 1px solid #8799ad;
            border-radius: 7px;
            padding: 10px 12px;
            selection-background-color: #2563eb;
            font-size: 14px;
        }

        QLineEdit:focus {
            border: 2px solid #2563eb;
        }

        QPushButton {
            background: #e4ebf3;
            color: #17365d;
            border: 1px solid #9bacc0;
            border-radius: 6px;
            padding: 8px 14px;
            font-weight: 600;
        }

        QPushButton:hover {
            background: #d3dfec;
            border-color: #6e849d;
        }

        QPushButton:disabled {
            color: #98a6b5;
            background: #f0f3f6;
            border-color: #d3dae2;
        }

        QPushButton#primary {
            background: #2563eb;
            color: #ffffff;
            border-color: #1d4ed8;
        }

        QPushButton#primary:hover {
            background: #1d4ed8;
        }

        QPushButton#danger {
            background: #b91c1c;
            color: #ffffff;
            border-color: #991b1b;
        }

        QTableView {
            background: #ffffff;
            alternate-background-color: #edf3f9;
            color: #172033;
            border: 1px solid #a9b7c8;
            border-radius: 6px;
            gridline-color: #d5dde6;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }

        QTableView::item {
            padding: 7px;
            border-bottom: 1px solid #dce3eb;
        }

        QHeaderView::section {
            background: #17365d;
            color: #ffffff;
            padding: 9px;
            border: none;
            border-right: 1px solid #496886;
            border-bottom: 1px solid #496886;
            font-weight: 700;
        }

        QProgressBar {
            background: #edf3f9;
            color: #17365d;
            border: 1px solid #9bacc0;
            border-radius: 5px;
            text-align: center;
            min-height: 20px;
        }

        QProgressBar::chunk {
            background: #2563eb;
            border-radius: 4px;
        }

        QStatusBar {
            background: #17365d;
            color: #ffffff;
            border-top: 1px solid #102a49;
        }
    )");
}

void applyTheme(
    QApplication& app,
    Theme theme)
{
    app.setStyleSheet(
        theme == Theme::Dark
            ? darkStyle()
            : lightStyle());
}

}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    const QIcon applicationIcon(
        QStringLiteral(":/efcrawler/efcrawler.png"));

    QApplication::setWindowIcon(applicationIcon);

    QApplication::setApplicationName(
        QStringLiteral("eFCrawler"));

    QApplication::setApplicationVersion(
        QString::fromLatin1(kVersion));

    QApplication::setOrganizationName(
        QStringLiteral("eFSoftware"));

    QSettings settings;

    const QString savedTheme =
        settings.value(
            QStringLiteral("appearance/theme"),
            QStringLiteral("dark"))
            .toString();

    Theme currentTheme =
        savedTheme == QStringLiteral("light")
            ? Theme::Light
            : Theme::Dark;

    applyTheme(app, currentTheme);

    QMainWindow window;

    window.setWindowTitle(
        QStringLiteral(
            "eFCrawler — Easy & Flexible Crawler"));

    window.resize(1180, 720);
    window.setMinimumSize(900, 600);

    // --------------------------------------------------------
    // Menus
    // --------------------------------------------------------

    QMenu* fileMenu =
        window.menuBar()->addMenu(
            QStringLiteral("&File"));

    QAction* openDownloadsAction =
        fileMenu->addAction(
            QStringLiteral(
                "Open &Downloads Folder"));

    fileMenu->addSeparator();

    QAction* quitAction =
        fileMenu->addAction(
            QStringLiteral("E&xit"));

    QMenu* researchMenu =
        window.menuBar()->addMenu(
            QStringLiteral("&Research"));

    QAction* startAction =
        researchMenu->addAction(
            QStringLiteral("&Start Research"));

    QAction* pauseAction =
        researchMenu->addAction(
            QStringLiteral("&Pause"));

    QAction* resumeAction =
        researchMenu->addAction(
            QStringLiteral("&Resume"));

    QAction* stopAction =
        researchMenu->addAction(
            QStringLiteral("S&top"));

    QMenu* toolsMenu =
        window.menuBar()->addMenu(
            QStringLiteral("&Tools"));

    QAction* clearResultsAction =
        toolsMenu->addAction(
            QStringLiteral("&Clear Results"));

    QMenu* appearanceMenu =
        toolsMenu->addMenu(
            QStringLiteral("&Appearance"));

    QAction* darkThemeAction =
        appearanceMenu->addAction(
            QStringLiteral("&Dark Theme"));

    QAction* lightThemeAction =
        appearanceMenu->addAction(
            QStringLiteral("&Light Theme"));

    darkThemeAction->setCheckable(true);
    lightThemeAction->setCheckable(true);

    darkThemeAction->setChecked(
        currentTheme == Theme::Dark);

    lightThemeAction->setChecked(
        currentTheme == Theme::Light);

    QMenu* helpMenu =
        window.menuBar()->addMenu(
            QStringLiteral("&Help"));

    QAction* aboutAction =
        helpMenu->addAction(
            QStringLiteral(
                "&About eFCrawler"));

    // --------------------------------------------------------
    // Main UI
    // --------------------------------------------------------

    auto* central = new QWidget(&window);
    central->setObjectName(QStringLiteral("central"));

    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(10);

    auto* header = new QFrame(central);
    header->setObjectName(
        QStringLiteral("headerCard"));

    auto* headerLayout =
        new QHBoxLayout(header);

    auto* brandColumn = new QVBoxLayout();

    auto* brand =
        new QLabel(
            QStringLiteral("eFCrawler"),
            header);

    brand->setObjectName(
        QStringLiteral("brand"));

    auto* subtitle =
        new QLabel(
            QStringLiteral(
                "Easy & Flexible Crawler  •  "
                "Autonomous Research and "
                "Resource Discovery"),
            header);

    subtitle->setObjectName(
        QStringLiteral("subtitle"));

    brandColumn->addWidget(brand);
    brandColumn->addWidget(subtitle);

    auto* versionLabel =
        new QLabel(
            QStringLiteral("C2R2  •  v%1")
                .arg(
                    QString::fromLatin1(
                        kVersion)),
            header);

    versionLabel->setObjectName(
        QStringLiteral("provider"));

    headerLayout->addLayout(brandColumn);
    headerLayout->addStretch();
    headerLayout->addWidget(versionLabel);

    // --------------------------------------------------------
    // Research controls
    // --------------------------------------------------------

    auto* researchCard =
        new QFrame(central);

    researchCard->setObjectName(
        QStringLiteral("researchCard"));

    auto* researchLayout =
        new QVBoxLayout(researchCard);

    auto* researchTitle =
        new QLabel(
            QStringLiteral("RESEARCH"),
            researchCard);

    researchTitle->setObjectName(
        QStringLiteral("sectionTitle"));

    auto* topic =
        new QLineEdit(researchCard);

    topic->setPlaceholderText(
        QStringLiteral(
            "What would you like "
            "eFCrawler to research?"));

    topic->setClearButtonEnabled(true);

    auto* optionsRow =
        new QHBoxLayout();

    auto* freeOnly =
        new QCheckBox(
            QStringLiteral(
                "Free resources only"),
            researchCard);

    const bool savedFreeOnly =
        settings.value(
            QStringLiteral(
                "research/freeOnly"),
            true)
            .toBool();

    freeOnly->setChecked(savedFreeOnly);

    auto* freeExplanation =
        new QLabel(
            QStringLiteral(
                "Prefer free, open-access "
                "and public-domain resources"),
            researchCard);

    freeExplanation->setObjectName(
        QStringLiteral("subtitle"));

    optionsRow->addWidget(freeOnly);
    optionsRow->addWidget(freeExplanation);
    optionsRow->addStretch();

    auto* buttonRow = new QHBoxLayout();

    auto* startButton =
        new QPushButton(
            QStringLiteral("Start Research"),
            researchCard);

    startButton->setObjectName(
        QStringLiteral("primary"));

    auto* pauseButton =
        new QPushButton(
            QStringLiteral("Pause"),
            researchCard);

    auto* resumeButton =
        new QPushButton(
            QStringLiteral("Resume"),
            researchCard);

    auto* stopButton =
        new QPushButton(
            QStringLiteral("Stop"),
            researchCard);

    stopButton->setObjectName(
        QStringLiteral("danger"));

    auto* providerLabel =
        new QLabel(
            QStringLiteral(
                "Provider: DuckDuckGo • Ready"),
            researchCard);

    providerLabel->setObjectName(
        QStringLiteral("provider"));

    auto* counterLabel =
        new QLabel(
            QStringLiteral("0 resources"),
            researchCard);

    counterLabel->setObjectName(
        QStringLiteral("counter"));

    buttonRow->addWidget(startButton);
    buttonRow->addWidget(pauseButton);
    buttonRow->addWidget(resumeButton);
    buttonRow->addWidget(stopButton);
    buttonRow->addStretch();
    buttonRow->addWidget(providerLabel);
    buttonRow->addWidget(counterLabel);

    researchLayout->addWidget(researchTitle);
    researchLayout->addWidget(topic);
    researchLayout->addLayout(optionsRow);
    researchLayout->addLayout(buttonRow);

    // --------------------------------------------------------
    // Results
    // --------------------------------------------------------

    auto* resultsCard =
        new QFrame(central);

    resultsCard->setObjectName(
        QStringLiteral("resultsCard"));

    auto* resultsLayout =
        new QVBoxLayout(resultsCard);

    auto* resultHeader =
        new QHBoxLayout();

    auto* resultsTitle =
        new QLabel(
            QStringLiteral("RESEARCH RESULTS"),
            resultsCard);

    resultsTitle->setObjectName(
        QStringLiteral("sectionTitle"));

    auto* openButton =
        new QPushButton(
            QStringLiteral("Open in Browser"),
            resultsCard);

    auto* downloadButton =
        new QPushButton(
            QStringLiteral("Download Selected"),
            resultsCard);

    auto* downloadsButton =
        new QPushButton(
            QStringLiteral("Downloads Folder"),
            resultsCard);

    resultHeader->addWidget(resultsTitle);
    resultHeader->addStretch();
    resultHeader->addWidget(openButton);
    resultHeader->addWidget(downloadButton);
    resultHeader->addWidget(downloadsButton);

    auto* results =
        new QTableView(resultsCard);

    results->setContextMenuPolicy(
        Qt::CustomContextMenu);

    auto* model =
        new QStandardItemModel(results);

    const auto setHeaders = [model]() {
        model->setHorizontalHeaderLabels({
            QStringLiteral("Title"),
            QStringLiteral("Type"),
            QStringLiteral("Source"),
            QStringLiteral("Size"),
            QStringLiteral("Access"),
            QStringLiteral("URL")
        });
    };

    setHeaders();

    results->setModel(model);

    results->setSelectionBehavior(
        QAbstractItemView::SelectRows);

    results->setSelectionMode(
        QAbstractItemView::SingleSelection);

    results->setEditTriggers(
        QAbstractItemView::NoEditTriggers);

    results->setAlternatingRowColors(true);
    results->setSortingEnabled(true);
    results->setShowGrid(false);

    results->verticalHeader()->setVisible(false);
    results->verticalHeader()
        ->setDefaultSectionSize(34);

    results->horizontalHeader()
        ->setSectionResizeMode(
            0,
            QHeaderView::Stretch);

    results->horizontalHeader()
        ->setSectionResizeMode(
            1,
            QHeaderView::ResizeToContents);

    results->horizontalHeader()
        ->setSectionResizeMode(
            2,
            QHeaderView::ResizeToContents);

    results->horizontalHeader()
        ->setSectionResizeMode(
            3,
            QHeaderView::ResizeToContents);

    results->horizontalHeader()
        ->setSectionResizeMode(
            4,
            QHeaderView::ResizeToContents);

    results->horizontalHeader()
        ->setSectionResizeMode(
            5,
            QHeaderView::Stretch);

    resultsLayout->addLayout(resultHeader);
    resultsLayout->addWidget(results);

    // --------------------------------------------------------
    // Status
    // --------------------------------------------------------

    auto* statusCard =
        new QFrame(central);

    statusCard->setObjectName(
        QStringLiteral("statusCard"));

    auto* statusLayout =
        new QVBoxLayout(statusCard);

    auto* activity =
        new QLabel(
            QStringLiteral(
                "Ready. Enter a research "
                "topic to begin."),
            statusCard);

    activity->setWordWrap(true);

    auto* progress =
        new QProgressBar(statusCard);

    progress->setRange(0, 6);
    progress->setValue(0);

    progress->setFormat(
        QStringLiteral(
            "Ready — 0 resources"));

    statusLayout->addWidget(activity);
    statusLayout->addWidget(progress);

    outer->addWidget(header);
    outer->addWidget(researchCard);
    outer->addWidget(resultsCard, 1);
    outer->addWidget(statusCard);

    window.setCentralWidget(central);

    // --------------------------------------------------------
    // Engine
    // --------------------------------------------------------

    efcrawler::ResearchEngine engine(&window);

    engine.setFreeOnly(
        freeOnly->isChecked());

    QObject::connect(
        freeOnly,
        &QCheckBox::toggled,
        &window,
        [&](bool enabled) {
            settings.setValue(
                QStringLiteral(
                    "research/freeOnly"),
                enabled);

            engine.setFreeOnly(enabled);

            window.statusBar()->showMessage(
                enabled
                    ? QStringLiteral(
                          "Free-resource research enabled.")
                    : QStringLiteral(
                          "General research enabled."),
                3000);
        });

    // --------------------------------------------------------
    // Theme selection
    // --------------------------------------------------------

    auto selectTheme =
        [&](Theme theme) {
            currentTheme = theme;

            applyTheme(app, theme);

            darkThemeAction->setChecked(
                theme == Theme::Dark);

            lightThemeAction->setChecked(
                theme == Theme::Light);

            settings.setValue(
                QStringLiteral(
                    "appearance/theme"),
                theme == Theme::Dark
                    ? QStringLiteral("dark")
                    : QStringLiteral("light"));

            settings.sync();
        };

    QObject::connect(
        darkThemeAction,
        &QAction::triggered,
        &window,
        [&]() {
            selectTheme(Theme::Dark);
        });

    QObject::connect(
        lightThemeAction,
        &QAction::triggered,
        &window,
        [&]() {
            selectTheme(Theme::Light);
        });

    // --------------------------------------------------------
    // Result helpers
    // --------------------------------------------------------

    auto selectedUrl =
        [results, model]() -> QUrl {
            const QModelIndex index =
                results->currentIndex();

            if (!index.isValid()) {
                return {};
            }

            return QUrl(
                model->data(
                    model->index(
                        index.row(),
                        5))
                    .toString());
        };

    auto openSelected =
        [&]() {
            const QUrl url = selectedUrl();

            if (!url.isValid() ||
                url.isEmpty()) {
                QMessageBox::information(
                    &window,
                    QStringLiteral("eFCrawler"),
                    QStringLiteral(
                        "Select a research "
                        "result first."));
                return;
            }

            if (!QDesktopServices::openUrl(url)) {
                QMessageBox::warning(
                    &window,
                    QStringLiteral("eFCrawler"),
                    QStringLiteral(
                        "Unable to open the "
                        "selected URL."));
            }
        };

    auto copySelectedUrl =
        [&]() {
            const QUrl url = selectedUrl();

            if (!url.isValid() ||
                url.isEmpty()) {
                return;
            }

            QApplication::clipboard()
                ->setText(url.toString());

            window.statusBar()->showMessage(
                QStringLiteral(
                    "URL copied to clipboard."),
                2500);
        };

    // --------------------------------------------------------
    // Download system
    // --------------------------------------------------------

    auto* downloadNetwork =
        new QNetworkAccessManager(&window);

    QNetworkReply* downloadReply = nullptr;
    QFile* downloadFile = nullptr;
    QString downloadFinalPath;
    QString downloadPartPath;

    auto downloadSelected =
        [&]() {
            if (downloadReply) {
                QMessageBox::information(
                    &window,
                    QStringLiteral("eFCrawler"),
                    QStringLiteral(
                        "A download is already "
                        "in progress."));
                return;
            }

            const QUrl url = selectedUrl();

            if (!url.isValid() ||
                url.isEmpty()) {
                QMessageBox::information(
                    &window,
                    QStringLiteral("eFCrawler"),
                    QStringLiteral(
                        "Select a research "
                        "result first."));
                return;
            }

            QDir().mkpath(
                downloadDirectory());

            const QString proposed =
                downloadDirectory() +
                QStringLiteral("/") +
                safeFileName(url);

            downloadFinalPath =
                QFileDialog::getSaveFileName(
                    &window,
                    QStringLiteral(
                        "Download Resource"),
                    proposed);

            if (downloadFinalPath.isEmpty()) {
                return;
            }

            downloadPartPath =
                downloadFinalPath +
                QStringLiteral(".part");

            QFile::remove(downloadPartPath);

            downloadFile =
                new QFile(
                    downloadPartPath,
                    &window);

            if (!downloadFile->open(
                    QIODevice::WriteOnly)) {
                QMessageBox::warning(
                    &window,
                    QStringLiteral(
                        "Download Failed"),
                    QStringLiteral(
                        "Unable to create:\n%1")
                        .arg(downloadPartPath));

                downloadFile->deleteLater();
                downloadFile = nullptr;
                return;
            }

            QNetworkRequest request(url);

            request.setAttribute(
                QNetworkRequest::
                    RedirectPolicyAttribute,
                QNetworkRequest::
                    NoLessSafeRedirectPolicy);

            request.setRawHeader(
                "User-Agent",
                "eFCrawler/0.2.2");

            downloadReply =
                downloadNetwork->get(request);

            activity->setText(
                QStringLiteral(
                    "Downloading: %1")
                    .arg(url.toString()));

            QObject::connect(
                downloadReply,
                &QNetworkReply::readyRead,
                &window,
                [&]() {
                    if (downloadReply &&
                        downloadFile) {
                        downloadFile->write(
                            downloadReply->readAll());
                    }
                });

            QObject::connect(
                downloadReply,
                &QNetworkReply::downloadProgress,
                &window,
                [&](qint64 received,
                    qint64 total) {
                    if (total > 0) {
                        activity->setText(
                            QStringLiteral(
                                "Downloading — "
                                "%1 / %2 KB")
                                .arg(received / 1024)
                                .arg(total / 1024));
                    }
                });

            QObject::connect(
                downloadReply,
                &QNetworkReply::finished,
                &window,
                [&]() {
                    QNetworkReply* finished =
                        downloadReply;

                    downloadReply = nullptr;

                    if (downloadFile) {
                        if (finished) {
                            downloadFile->write(
                                finished->readAll());
                        }

                        downloadFile->close();
                    }

                    if (!finished) {
                        return;
                    }

                    if (finished->error() !=
                        QNetworkReply::NoError) {
                        QFile::remove(
                            downloadPartPath);

                        QMessageBox::warning(
                            &window,
                            QStringLiteral(
                                "Download Failed"),
                            finished->errorString());
                    } else {
                        QFile::remove(
                            downloadFinalPath);

                        if (!QFile::rename(
                                downloadPartPath,
                                downloadFinalPath)) {
                            QMessageBox::warning(
                                &window,
                                QStringLiteral(
                                    "Download Failed"),
                                QStringLiteral(
                                    "The temporary download "
                                    "could not be renamed."));
                        } else {
                            activity->setText(
                                QStringLiteral(
                                    "Download complete: %1")
                                    .arg(
                                        downloadFinalPath));

                            window.statusBar()
                                ->showMessage(
                                    QStringLiteral(
                                        "Download complete."),
                                    5000);
                        }
                    }

                    if (downloadFile) {
                        downloadFile->deleteLater();
                        downloadFile = nullptr;
                    }

                    finished->deleteLater();
                });
        };

    // --------------------------------------------------------
    // Main research controls
    // --------------------------------------------------------

    auto updateControls =
        [&]() {
            using State =
                efcrawler::ResearchEngine::State;

            const State state =
                engine.state();

            const bool idle =
                state == State::Idle;

            const bool searching =
                state == State::Searching;

            const bool paused =
                state == State::Paused;

            startButton->setEnabled(idle);
            startAction->setEnabled(idle);

            pauseButton->setEnabled(searching);
            pauseAction->setEnabled(searching);

            resumeButton->setEnabled(paused);
            resumeAction->setEnabled(paused);

            stopButton->setEnabled(!idle);
            stopAction->setEnabled(!idle);

            freeOnly->setEnabled(idle);
        };

    auto startResearch =
        [&]() {
            engine.setFreeOnly(
                freeOnly->isChecked());

            engine.startResearch(
                topic->text());
        };

    QObject::connect(
        startButton,
        &QPushButton::clicked,
        &window,
        startResearch);

    QObject::connect(
        startAction,
        &QAction::triggered,
        &window,
        startResearch);

    QObject::connect(
        topic,
        &QLineEdit::returnPressed,
        &window,
        startResearch);

    QObject::connect(
        pauseButton,
        &QPushButton::clicked,
        &engine,
        &efcrawler::ResearchEngine::pauseResearch);

    QObject::connect(
        pauseAction,
        &QAction::triggered,
        &engine,
        &efcrawler::ResearchEngine::pauseResearch);

    QObject::connect(
        resumeButton,
        &QPushButton::clicked,
        &engine,
        &efcrawler::ResearchEngine::resumeResearch);

    QObject::connect(
        resumeAction,
        &QAction::triggered,
        &engine,
        &efcrawler::ResearchEngine::resumeResearch);

    QObject::connect(
        stopButton,
        &QPushButton::clicked,
        &engine,
        &efcrawler::ResearchEngine::stopResearch);

    QObject::connect(
        stopAction,
        &QAction::triggered,
        &engine,
        &efcrawler::ResearchEngine::stopResearch);

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::resultsCleared,
        &window,
        [&]() {
            model->clear();
            setHeaders();

            counterLabel->setText(
                QStringLiteral(
                    "0 resources"));
        });

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::resultDiscovered,
        &window,
        [&](const efcrawler::SearchResult& result) {
            auto* titleItem =
                new QStandardItem(
                    result.title);

            auto* typeItem =
                new QStandardItem(
                    result.type);

            auto* sourceItem =
                new QStandardItem(
                    result.source);

            auto* sizeItem =
                new QStandardItem(
                    result.size);

            auto* accessItem =
                new QStandardItem(
                    result.access);

            auto* urlItem =
                new QStandardItem(
                    result.url.toString());

            QFont linkFont =
                urlItem->font();

            linkFont.setUnderline(true);

            urlItem->setFont(linkFont);
            urlItem->setForeground(
                QColor(QStringLiteral(
                    "#2563eb")));

            urlItem->setToolTip(
                QStringLiteral(
                    "Double-click to open "
                    "in your web browser"));

            QList<QStandardItem*> row;

            row << titleItem
                << typeItem
                << sourceItem
                << sizeItem
                << accessItem
                << urlItem;

            model->appendRow(row);

            counterLabel->setText(
                QStringLiteral(
                    "%1 resource%2")
                    .arg(model->rowCount())
                    .arg(
                        model->rowCount() == 1
                            ? QString()
                            : QStringLiteral("s")));
        });

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::statusChanged,
        &window,
        [&](const QString& message) {
            activity->setText(message);

            window.statusBar()
                ->showMessage(message);
        });

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::
            providerStatusChanged,
        &window,
        [&](const QString& provider,
            const QString& status) {
            providerLabel->setText(
                QStringLiteral(
                    "Provider: %1 • %2")
                    .arg(provider, status));
        });

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::
            progressChanged,
        &window,
        [&](int completed,
            int total,
            int found) {
            progress->setRange(
                0,
                qMax(total, 1));

            progress->setValue(completed);

            progress->setFormat(
                QStringLiteral(
                    "%1 / %2 research queries  •  "
                    "%3 resource(s)")
                    .arg(completed)
                    .arg(total)
                    .arg(found));
        });

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::
            researchStarted,
        &window,
        [&](const QString&) {
            updateControls();
        });

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::
            researchPaused,
        &window,
        updateControls);

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::
            researchResumed,
        &window,
        updateControls);

    QObject::connect(
        &engine,
        &efcrawler::ResearchEngine::
            researchStopped,
        &window,
        updateControls);

    // --------------------------------------------------------
    // Result actions
    // --------------------------------------------------------

    QObject::connect(
        openButton,
        &QPushButton::clicked,
        &window,
        openSelected);

    QObject::connect(
        downloadButton,
        &QPushButton::clicked,
        &window,
        downloadSelected);

    QObject::connect(
        results,
        &QTableView::doubleClicked,
        &window,
        [&](const QModelIndex&) {
            openSelected();
        });

    QObject::connect(
        results,
        &QWidget::customContextMenuRequested,
        &window,
        [&](const QPoint& position) {
            const QModelIndex index =
                results->indexAt(position);

            if (!index.isValid()) {
                return;
            }

            results->selectRow(index.row());

            QMenu menu(results);

            QAction* open =
                menu.addAction(
                    QStringLiteral(
                        "Open in Browser"));

            QAction* download =
                menu.addAction(
                    QStringLiteral(
                        "Download Selected"));

            menu.addSeparator();

            QAction* copy =
                menu.addAction(
                    QStringLiteral(
                        "Copy URL"));

            QAction* chosen =
                menu.exec(
                    results->viewport()
                        ->mapToGlobal(position));

            if (chosen == open) {
                openSelected();
            } else if (chosen == download) {
                downloadSelected();
            } else if (chosen == copy) {
                copySelectedUrl();
            }
        });

    auto openDownloads =
        [&]() {
            QDir().mkpath(
                downloadDirectory());

            QDesktopServices::openUrl(
                QUrl::fromLocalFile(
                    downloadDirectory()));
        };

    QObject::connect(
        downloadsButton,
        &QPushButton::clicked,
        &window,
        openDownloads);

    QObject::connect(
        openDownloadsAction,
        &QAction::triggered,
        &window,
        openDownloads);

    QObject::connect(
        clearResultsAction,
        &QAction::triggered,
        &window,
        [&]() {
            if (engine.state() !=
                efcrawler::ResearchEngine::
                    State::Idle) {
                QMessageBox::information(
                    &window,
                    QStringLiteral(
                        "eFCrawler"),
                    QStringLiteral(
                        "Stop active research "
                        "before clearing results."));
                return;
            }

            model->removeRows(
                0,
                model->rowCount());

            counterLabel->setText(
                QStringLiteral(
                    "0 resources"));
        });

    // --------------------------------------------------------
    // About
    // --------------------------------------------------------

    QObject::connect(
        aboutAction,
        &QAction::triggered,
        &window,
        [&]() {
            QMessageBox about(&window);

            about.setWindowTitle(
                QStringLiteral(
                    "About eFCrawler"));

            about.setIcon(
                QMessageBox::Information);

            about.setText(
                QStringLiteral(
                    "<h2>eFCrawler</h2>"
                    "<p><b>Easy &amp; Flexible "
                    "Crawler</b></p>"
                    "<p>Autonomous Research and "
                    "Resource Discovery</p>"
                    "<table>"
                    "<tr><td><b>Program Date:"
                    "</b></td><td>%1</td></tr>"
                    "<tr><td><b>Program Version:"
                    "</b></td><td>%2</td></tr>"
                    "<tr><td><b>Author:</b></td>"
                    "<td>Dr. Eric O. Flores</td></tr>"
                    "<tr><td><b>E-mail:</b></td>"
                    "<td>eoftoro@gmail.com</td></tr>"
                    "</table>"
                    "<p>Native C++23 / Qt6 "
                    "application.</p>")
                    .arg(
                        QString::fromLatin1(
                            kProgramDate),
                        QString::fromLatin1(
                            kVersion)));

            about.exec();
        });

    QObject::connect(
        quitAction,
        &QAction::triggered,
        &window,
        &QMainWindow::close);

    updateControls();

    window.statusBar()->showMessage(
        QStringLiteral(
            "Ready — eFCrawler C2R2"));

    window.show();

    return app.exec();
}
