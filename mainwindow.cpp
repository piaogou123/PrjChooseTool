/*
 * @CopyRight: iNovatrol
 * @Description: Implementation of the project-variant chooser window
 * @version: <SET-YOUR-INITIALS> <SET-YOUR-VERSION>   // e.g. JCK J01
 * @Author: <SET-YOUR-NAME-IN-PINYIN>
 * @Date: 2026.06.15
 */

#include "mainwindow.h"   // HD-10: own header first

#include <cstdint>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWidget>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <tlhelp32.h>
#endif

namespace prjchoosetool
{

namespace
{

// Folder prefix that marks a user-data variant folder.
const QString FOLDER_PREFIX = QStringLiteral("Data_User.");

// Sub-path of the definition file relative to the working directory.
const QString DEFINITION_SUBDIR  = QStringLiteral("Data_System");
const QString DEFINITION_FILE    = QStringLiteral("ProjectDefinition.dat");

// QSettings keys.
const QString SETTINGS_LAST_DIR     = QStringLiteral("workingDir/lastPath");
const QString SETTINGS_LANG         = QStringLiteral("ui/lang");
const QString SETTINGS_LIC_ENABLED  = QStringLiteral("license/enabled");
const QString SETTINGS_LIC_KEYPREF  = QStringLiteral("license/key/");

// Sub-path of the license file inside a variant folder.
const QString LICENSE_SUBPATH = QStringLiteral("Settings/license");

// Item data roles for tree rows.
const int32_t ROLE_FOLDER  = Qt::UserRole;       // full Data_User.* folder
const int32_t ROLE_PROJECT = Qt::UserRole + 1;   // owning project id

// Language codes.
const int32_t LANG_EN = 0;
const int32_t LANG_ZH = 1;

// GeekUninstaller-like flat, native, light theme.
const char* const APP_STYLE =
    "QMainWindow, QWidget { background: #FFFFFF; color: #000000;"
    "  font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;"
    "  font-size: 9pt; }"
    "QMenuBar { background: #F5F5F5; border-bottom: 1px solid #DCDCDC; }"
    "QMenuBar::item { padding: 4px 10px; background: transparent; }"
    "QMenuBar::item:selected { background: #CBE8F6; }"
    "QMenu { background: #FFFFFF; border: 1px solid #ABADB3; }"
    "QMenu::item { padding: 4px 24px; }"
    "QMenu::item:selected { background: #CBE8F6; }"
    "QLineEdit { border: 1px solid #ABADB3; padding: 2px 4px;"
    "  background: #FFFFFF; }"
    "QLineEdit:focus { border: 1px solid #3399FF; }"
    "QPushButton { border: 1px solid #ABADB3; padding: 3px 12px;"
    "  background: #F5F5F5; min-height: 18px; }"
    "QPushButton:hover { background: #E5F1FB; border-color: #3399FF; }"
    "QPushButton:pressed { background: #CCE4F7; }"
    "QPushButton:default { border-color: #3399FF; }"
    "QTreeWidget { border: 1px solid #ABADB3; outline: 0;"
    "  alternate-background-color: #F7F7F7; }"
    "QTreeView::item { height: 20px; border: 0px; }"
    "QTreeView::item:selected { background: #CBE8F6; color: #000000; }"
    "QTreeView::item:selected:!active { background: #E8E8E8; }"
    "QHeaderView::section { background: #F5F5F5; padding: 3px 6px;"
    "  border: 0px; border-right: 1px solid #E2E2E2;"
    "  border-bottom: 1px solid #DCDCDC; }"
    "QStatusBar { background: #F5F5F5; border-top: 1px solid #DCDCDC; }"
    "QStatusBar::item { border: 0px; }";

// Extract the project id from a folder name such as
// "Data_User.F306.WFiber2" -> "F306". Returns an empty string if the
// folder does not match the expected pattern.
QString ParseProjectId(const QString& folderName)
{
    if (!folderName.startsWith(FOLDER_PREFIX)) {
        return QString();
    }

    const QString remainder = folderName.mid(FOLDER_PREFIX.length());
    const int32_t dotPos = static_cast<int32_t>(remainder.indexOf('.'));
    if (dotPos <= 0) {
        // Need <ProjectId>.<Variant>.
        return QString();
    }

    const QString projectId = remainder.left(dotPos).trimmed();
    const QString variant   = remainder.mid(dotPos + 1).trimmed();
    if (projectId.isEmpty() || variant.isEmpty()) {
        return QString();
    }
    return projectId;
}

}  // namespace

MainWindow::MainWindow(QWidget* p_parent)
    : QMainWindow(p_parent)
{
    QSettings settings;
    m_lang = settings.value(SETTINGS_LANG, LANG_EN).toInt();
    m_licenseProjects = settings.value(SETTINGS_LIC_ENABLED).toStringList();
    m_licenseProjects.sort();

    BuildTranslations();
    BuildUi();
    BuildMenu();
    SetupTray();
    RetranslateUi();
    BuildLicenseRows();
    OnReload();
    RunAllLicenseChecks();   // first-launch verification
}

MainWindow::~MainWindow()
{
    // Child widgets and button groups are owned by Qt's parent hierarchy,
    // so no manual cleanup is required here.
}

void MainWindow::BuildTranslations()
{
    auto add = [this](const char* en, const char* zh) {
        m_zh.insert(QString::fromLatin1(en), QString::fromUtf8(zh));
    };

    add("Project Chooser", u8"项目选择器");
    add("Folder:", u8"文件夹:");
    add("Browse...", u8"浏览...");
    add("Reload", u8"刷新");
    add("Set as active", u8"设为启用");
    add("Language", u8"语言");
    add("Select your working folder...",
        u8"请选择工作目录...");
    add("Project / Variant", u8"项目 / 配置");
    add("Status", u8"状态");
    add("Ready", u8"就绪");
    add("Active", u8"启用中");
    add("missing", u8"缺失");

    add("&File", u8"文件(&F)");
    add("&Reload", u8"刷新(&R)");
    add("E&xit", u8"退出(&X)");
    add("&Actions", u8"操作(&A)");
    add("&Set as active", u8"设为启用(&S)");
    add("&Help", u8"帮助(&H)");
    add("&About", u8"关于(&A)");

    add("Show window", u8"显示窗口");
    add("Quit", u8"退出");
    add("Still running in the tray. Right-click the icon to quit.",
        u8"已最小化到托盘后台运"
        u8"行。右键托盘图标可退"
        u8"出。");

    add("Select a working folder (Browse...).",
        u8"请选择工作目录(点浏"
        u8"览...)。");
    add("Working folder does not exist: ",
        u8"工作目录不存在: ");
    add("%1 project(s), %2 variant(s).",
        u8"%1 个项目，%2 个配置。");
    add("  ProjectDefinition.dat not found.",
        u8"  未找到 ProjectDefinition.dat。");
    add("  Duplicate ids: ", u8"  重复的项目号: ");
    add("Select a variant under a project.",
        u8"请在某个项目下选择一"
        u8"个配置。");
    add("Target: project %1 -> %2  (click \"Set as active\")",
        u8"目标: 项目 %1 -> %2  "
        u8"(点“设为启用”)");
    add("Select a variant first.",
        u8"请先选择一个配置。");
    add("Select a variant row, not a project.",
        u8"请选择配置行，而不是"
        u8"项目行。");
    add("Set %1 -> %2   (%3)",
        u8"已设置 %1 -> %2   (%3)");

    add("Program is running", u8"程序正在运行");
    add("\"i-Novatrol %1\" is currently running.\n"
        "Please close that program before changing project %1.",
        u8"“i-Novatrol %1” 正在运行。\n"
        u8"请先关闭该程序，再修"
        u8"改项目 %1。");
    add("Cannot change %1: program is running.",
        u8"无法修改 %1: 程序正在运"
        u8"行。");

    add("Update failed", u8"修改失败");
    add("About Project Chooser",
        u8"关于 项目选择器");
    add("<b>Project Chooser</b><br>"
        "Switches the active Data_User variant for a project "
        "by editing Data_System/ProjectDefinition.dat.<br><br>"
        "iNovatrol",
        u8"<b>项目选择器</b><br>"
        u8"通过编辑 Data_System/ProjectDefinition.dat "
        u8"切换某个项目当前启用"
        u8"的 Data_User 配置。<br><br>iNovatrol");

    add("File not found: ", u8"文件不存在: ");
    add("Cannot open for reading: ",
        u8"无法读取文件: ");
    add("Cannot open for writing: ",
        u8"无法写入文件: ");
    add("Updated %1 line(s). Backup: %2",
        u8"已更新 %1 行。备份: %2");
    add(" | Duplicate ids left untouched: ",
        u8" | 重复项目号未改动: ");
    add(" | No matching line for: ",
        u8" | 未找到对应行: ");

    add("License settings...", u8"License 设置...");
    add("License check settings", u8"License 校验设置");
    add("Select the projects that require a license check:",
        u8"勾选需要 license 校验的项目:");
    add("(No projects found. Reload a folder first.)",
        u8"(没有项目。请先加载一"
        u8"个目录。)");
    add("License mismatch", u8"License 不一致");
    add("Project %1 license does not match the entered key.\n"
        "Write the current key to:\n%2 ?",
        u8"项目 %1 的 license 与输入"
        u8"的密钥不一致。\n"
        u8"是否将当前密钥写入到:\n%2 ？");
    add("License written for %1.", u8"已写入 %1 的 license。");
    add("Failed to write license: %1",
        u8"写入 license 失败: %1");
}

QString MainWindow::Tr(const QString& english) const
{
    if (m_lang == LANG_ZH) {
        return m_zh.value(english, english);
    }
    return english;
}

void MainWindow::BuildUi()
{
    setWindowIcon(QIcon(QStringLiteral(":/appicon.png")));
    setStyleSheet(QString::fromUtf8(APP_STYLE));
    resize(580, 460);

    QWidget* p_central = new QWidget(this);
    setCentralWidget(p_central);

    QVBoxLayout* p_mainLayout = new QVBoxLayout(p_central);
    p_mainLayout->setContentsMargins(8, 8, 8, 8);
    p_mainLayout->setSpacing(6);

    // --- Top row: folder + Browse + Reload + Language ---------------
    QHBoxLayout* p_topRow = new QHBoxLayout();
    p_topRow->setSpacing(6);

    m_pDirLabel = new QLabel(p_central);

    QSettings settings;
    const QString savedDir = settings.value(SETTINGS_LAST_DIR).toString();
    m_pBaseDirEdit = new QLineEdit(savedDir, p_central);

    m_pBrowseButton = new QPushButton(p_central);
    m_pReloadButton = new QPushButton(p_central);

    p_topRow->addWidget(m_pDirLabel);
    p_topRow->addWidget(m_pBaseDirEdit, 1);
    p_topRow->addWidget(m_pBrowseButton);
    p_topRow->addWidget(m_pReloadButton);
    p_mainLayout->addLayout(p_topRow);

    // --- License inputs (one row per enabled project) --------------
    m_pLicenseContainer = new QWidget(p_central);
    m_pLicenseLayout = new QVBoxLayout(m_pLicenseContainer);
    m_pLicenseLayout->setContentsMargins(0, 0, 0, 0);
    m_pLicenseLayout->setSpacing(4);
    p_mainLayout->addWidget(m_pLicenseContainer);

    // --- Project / variant tree ------------------------------------
    m_pTree = new QTreeWidget(p_central);
    m_pTree->setColumnCount(2);
    m_pTree->setRootIsDecorated(true);
    m_pTree->setUniformRowHeights(true);
    m_pTree->setAlternatingRowColors(true);
    m_pTree->setAllColumnsShowFocus(true);
    m_pTree->setExpandsOnDoubleClick(false);
    m_pTree->header()->setStretchLastSection(false);
    m_pTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_pTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    p_mainLayout->addWidget(m_pTree, 1);

    // --- Bottom row: Set-active button -----------------------------
    QHBoxLayout* p_bottomRow = new QHBoxLayout();
    p_bottomRow->addStretch(1);

    m_pSetButton = new QPushButton(p_central);
    m_pSetButton->setDefault(true);
    p_bottomRow->addWidget(m_pSetButton);
    p_mainLayout->addLayout(p_bottomRow);

    connect(m_pBrowseButton, &QPushButton::clicked,
            this, &MainWindow::OnBrowse);
    connect(m_pReloadButton, &QPushButton::clicked,
            this, &MainWindow::OnReload);
    connect(m_pSetButton, &QPushButton::clicked,
            this, &MainWindow::OnSetActive);
    connect(m_pTree, &QTreeWidget::itemActivated,
            this, &MainWindow::OnTreeActivated);
    connect(m_pTree, &QTreeWidget::currentItemChanged,
            this, &MainWindow::OnCurrentItemChanged);
}

void MainWindow::BuildMenu()
{
    m_pFileMenu = menuBar()->addMenu(QString());
    m_pReloadAction = m_pFileMenu->addAction(QString());
    m_pReloadAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    m_pFileMenu->addSeparator();
    m_pExitAction = m_pFileMenu->addAction(QString());

    m_pActionsMenu = menuBar()->addMenu(QString());
    m_pSetAction = m_pActionsMenu->addAction(QString());
    m_pSetAction->setShortcut(QKeySequence(QStringLiteral("Return")));

    m_pHelpMenu = menuBar()->addMenu(QString());

    // Help -> Language -> (English / 中文), exclusive, current is checked.
    m_pLangMenu = m_pHelpMenu->addMenu(QString());
    QActionGroup* p_langGroup = new QActionGroup(this);
    p_langGroup->setExclusive(true);

    m_pLangEnAction = m_pLangMenu->addAction(QStringLiteral("English"));
    m_pLangEnAction->setCheckable(true);
    m_pLangEnAction->setData(LANG_EN);
    p_langGroup->addAction(m_pLangEnAction);

    m_pLangZhAction = m_pLangMenu->addAction(QString::fromUtf8(u8"中文"));
    m_pLangZhAction->setCheckable(true);
    m_pLangZhAction->setData(LANG_ZH);
    p_langGroup->addAction(m_pLangZhAction);

    m_pHelpMenu->addSeparator();
    m_pLicenseSettingsAction = m_pHelpMenu->addAction(QString());
    m_pHelpMenu->addSeparator();
    m_pAboutAction = m_pHelpMenu->addAction(QString());

    connect(m_pReloadAction, &QAction::triggered, this, &MainWindow::OnReload);
    connect(m_pExitAction, &QAction::triggered, this, &MainWindow::QuitApp);
    connect(m_pSetAction, &QAction::triggered, this, &MainWindow::OnSetActive);
    connect(m_pAboutAction, &QAction::triggered, this, &MainWindow::OnAbout);
    connect(m_pLicenseSettingsAction, &QAction::triggered,
            this, &MainWindow::OnLicenseSettings);
    connect(p_langGroup, &QActionGroup::triggered,
            this, &MainWindow::OnLanguageSelected);
}

void MainWindow::SetupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        // No tray: closing will quit normally (see closeEvent).
        return;
    }

    // Keep the app alive after the window is hidden to the tray.
    QApplication::setQuitOnLastWindowClosed(false);

    m_pTrayIcon = new QSystemTrayIcon(
        QIcon(QStringLiteral(":/appicon.png")), this);

    QMenu* p_menu = new QMenu(this);
    m_pTrayShowAction = p_menu->addAction(QString());
    p_menu->addSeparator();
    m_pTrayQuitAction = p_menu->addAction(QString());

    m_pTrayIcon->setContextMenu(p_menu);
    m_pTrayIcon->show();

    connect(m_pTrayShowAction, &QAction::triggered,
            this, &MainWindow::ShowFromTray);
    connect(m_pTrayQuitAction, &QAction::triggered,
            this, &MainWindow::QuitApp);
    connect(m_pTrayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::OnTrayActivated);
}

void MainWindow::RetranslateUi()
{
    setWindowTitle(Tr(QStringLiteral("Project Chooser")));
    m_pDirLabel->setText(Tr(QStringLiteral("Folder:")));
    m_pBaseDirEdit->setPlaceholderText(
        Tr(QStringLiteral("Select your working folder...")));
    m_pBrowseButton->setText(Tr(QStringLiteral("Browse...")));
    m_pReloadButton->setText(Tr(QStringLiteral("Reload")));
    m_pSetButton->setText(Tr(QStringLiteral("Set as active")));

    QStringList headers;
    headers << Tr(QStringLiteral("Project / Variant"))
            << Tr(QStringLiteral("Status"));
    m_pTree->setHeaderLabels(headers);

    m_pFileMenu->setTitle(Tr(QStringLiteral("&File")));
    m_pActionsMenu->setTitle(Tr(QStringLiteral("&Actions")));
    m_pHelpMenu->setTitle(Tr(QStringLiteral("&Help")));
    m_pReloadAction->setText(Tr(QStringLiteral("&Reload")));
    m_pExitAction->setText(Tr(QStringLiteral("E&xit")));
    m_pSetAction->setText(Tr(QStringLiteral("&Set as active")));
    m_pAboutAction->setText(Tr(QStringLiteral("&About")));
    m_pLangMenu->setTitle(Tr(QStringLiteral("Language")));
    m_pLangEnAction->setChecked(m_lang == LANG_EN);
    m_pLangZhAction->setChecked(m_lang == LANG_ZH);
    m_pLicenseSettingsAction->setText(
        Tr(QStringLiteral("License settings...")));

    if (m_pTrayShowAction != nullptr) {
        m_pTrayShowAction->setText(Tr(QStringLiteral("Show window")));
    }
    if (m_pTrayQuitAction != nullptr) {
        m_pTrayQuitAction->setText(Tr(QStringLiteral("Quit")));
    }
    if (m_pTrayIcon != nullptr) {
        m_pTrayIcon->setToolTip(Tr(QStringLiteral("Project Chooser")));
    }
}

void MainWindow::OnLanguageSelected(QAction* p_action)
{
    if (p_action == nullptr) {
        return;
    }
    const int32_t lang = p_action->data().toInt();
    if (lang == m_lang) {
        return;
    }

    m_lang = lang;
    QSettings settings;
    settings.setValue(SETTINGS_LANG, m_lang);

    RetranslateUi();
    OnReload();   // refresh tree + status text in the new language
}

void MainWindow::BuildLicenseRows()
{
    m_licenseEdits.clear();

    // Remove any existing rows.
    QLayoutItem* p_item = nullptr;
    while ((p_item = m_pLicenseLayout->takeAt(0)) != nullptr) {
        if (p_item->widget() != nullptr) {
            p_item->widget()->deleteLater();
        }
        delete p_item;
    }

    QSettings settings;
    for (const QString& projectId : m_licenseProjects) {
        QWidget* p_row = new QWidget(m_pLicenseContainer);
        QHBoxLayout* p_rowLayout = new QHBoxLayout(p_row);
        p_rowLayout->setContentsMargins(0, 0, 0, 0);
        p_rowLayout->setSpacing(6);

        QLabel* p_label =
            new QLabel(QStringLiteral("License %1:").arg(projectId), p_row);
        QLineEdit* p_edit = new QLineEdit(p_row);
        p_edit->setProperty("projectId", projectId);
        p_edit->setEchoMode(QLineEdit::Password);   // mask the key
        p_edit->setText(
            settings.value(SETTINGS_LIC_KEYPREF + projectId).toString());

        p_rowLayout->addWidget(p_label);
        p_rowLayout->addWidget(p_edit, 1);
        m_pLicenseLayout->addWidget(p_row);
        m_licenseEdits.insert(projectId, p_edit);

        connect(p_edit, &QLineEdit::editingFinished,
                this, &MainWindow::OnLicenseKeyEdited);
    }

    m_pLicenseContainer->setVisible(!m_licenseProjects.isEmpty());
}

void MainWindow::OnLicenseKeyEdited()
{
    QLineEdit* p_edit = qobject_cast<QLineEdit*>(sender());
    if (p_edit == nullptr) {
        return;
    }
    const QString projectId = p_edit->property("projectId").toString();
    if (projectId.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.setValue(SETTINGS_LIC_KEYPREF + projectId,
                      p_edit->text().trimmed());
}

QString MainWindow::LicenseFilePath(const QString& projectId) const
{
    const QString activeFolder = m_activeVariants.value(projectId);
    if (activeFolder.isEmpty()) {
        return QString();
    }
    const QString baseDir = m_pBaseDirEdit->text().trimmed();
    return QDir(baseDir).filePath(activeFolder + "/" + LICENSE_SUBPATH);
}

void MainWindow::RunLicenseCheck(const QString& projectId)
{
    QLineEdit* p_edit = m_licenseEdits.value(projectId, nullptr);
    if (p_edit == nullptr) {
        return;
    }
    const QString key = p_edit->text().trimmed();
    if (key.isEmpty()) {
        return;   // nothing entered to compare against
    }

    const QString path = LicenseFilePath(projectId);
    if (path.isEmpty()) {
        return;   // no active variant for this project
    }

    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return;   // Settings/license missing -> skip (per requirement)
    }
    const QString current = QString::fromUtf8(file.readAll()).trimmed();
    file.close();

    if (current == key) {
        return;   // already matches
    }

    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, Tr(QStringLiteral("License mismatch")),
        Tr(QStringLiteral(
               "Project %1 license does not match the entered key.\n"
               "Write the current key to:\n%2 ?"))
            .arg(projectId, QDir::toNativeSeparators(path)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes) {
        return;
    }

    QFile outFile(path);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        SetStatus(Tr(QStringLiteral("Failed to write license: %1")).arg(path));
        return;
    }
    outFile.write(key.toUtf8());
    outFile.close();
    SetStatus(Tr(QStringLiteral("License written for %1.")).arg(projectId));
}

void MainWindow::RunAllLicenseChecks()
{
    for (const QString& projectId : m_licenseProjects) {
        RunLicenseCheck(projectId);
    }
}

void MainWindow::OnLicenseSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(Tr(QStringLiteral("License check settings")));

    QVBoxLayout* p_layout = new QVBoxLayout(&dialog);
    p_layout->addWidget(new QLabel(
        Tr(QStringLiteral(
            "Select the projects that require a license check:")),
        &dialog));

    // Union of discovered projects and already-enabled ones.
    QStringList ids = m_projects.keys();
    for (const QString& enabled : m_licenseProjects) {
        if (!ids.contains(enabled)) {
            ids.append(enabled);
        }
    }
    ids.sort();

    QList<QCheckBox*> boxes;
    if (ids.isEmpty()) {
        p_layout->addWidget(new QLabel(
            Tr(QStringLiteral(
                "(No projects found. Reload a folder first.)")),
            &dialog));
    } else {
        for (const QString& id : ids) {
            QCheckBox* p_box = new QCheckBox(id, &dialog);
            p_box->setChecked(m_licenseProjects.contains(id));
            p_layout->addWidget(p_box);
            boxes.append(p_box);
        }
    }

    QDialogButtonBox* p_buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    p_layout->addWidget(p_buttons);
    connect(p_buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(p_buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList selected;
    for (QCheckBox* p_box : boxes) {
        if (p_box->isChecked()) {
            selected.append(p_box->text());
        }
    }
    selected.sort();

    m_licenseProjects = selected;
    QSettings settings;
    settings.setValue(SETTINGS_LIC_ENABLED, m_licenseProjects);

    BuildLicenseRows();
}

void MainWindow::closeEvent(QCloseEvent* p_event)
{
    if (p_event == nullptr) {
        return;
    }

    // A real quit (from the tray menu) or no tray available -> close.
    if (m_forceQuit || m_pTrayIcon == nullptr) {
        p_event->accept();
        return;
    }

    // Otherwise hide to the tray and keep running in the background.
    hide();
    p_event->ignore();

    if (!m_trayHintShown) {
        m_pTrayIcon->showMessage(
            Tr(QStringLiteral("Project Chooser")),
            Tr(QStringLiteral("Still running in the tray. "
                              "Right-click the icon to quit.")),
            QSystemTrayIcon::Information, 3000);
        m_trayHintShown = true;
    }
}

void MainWindow::OnTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger
        || reason == QSystemTrayIcon::DoubleClick) {
        ShowFromTray();
    }
}

void MainWindow::ShowFromTray()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::QuitApp()
{
    m_forceQuit = true;
    QApplication::quit();
}

void MainWindow::OnAbout()
{
    QMessageBox::about(
        this, Tr(QStringLiteral("About Project Chooser")),
        Tr(QStringLiteral("<b>Project Chooser</b><br>"
                          "Switches the active Data_User variant for a project "
                          "by editing Data_System/ProjectDefinition.dat.<br><br>"
                          "iNovatrol")));
}

QString MainWindow::DatFilePath() const
{
    const QString baseDir = m_pBaseDirEdit->text().trimmed();
    return QDir(baseDir).filePath(
        DEFINITION_SUBDIR + "/" + DEFINITION_FILE);
}

void MainWindow::SetStatus(const QString& text)
{
    statusBar()->showMessage(text);
}

void MainWindow::OnBrowse()
{
    const QString chosen = QFileDialog::getExistingDirectory(
        this, Tr(QStringLiteral("Select your working folder...")),
        m_pBaseDirEdit->text().trimmed());
    if (!chosen.isEmpty()) {
        m_pBaseDirEdit->setText(chosen);
        OnReload();
    }
}

void MainWindow::OnReload()
{
    m_projects.clear();
    m_activeVariants.clear();

    const QString baseDir = m_pBaseDirEdit->text().trimmed();
    if (baseDir.isEmpty()) {
        SetStatus(Tr(QStringLiteral("Select a working folder (Browse...).")));
        PopulateTree();
        return;
    }
    if (!QDir(baseDir).exists()) {
        SetStatus(Tr(QStringLiteral("Working folder does not exist: "))
                  + baseDir);
        PopulateTree();
        return;
    }

    // Remember this folder for next launch.
    QSettings settings;
    settings.setValue(SETTINGS_LAST_DIR, baseDir);

    ScanProjects(baseDir, &m_projects);

    QStringList duplicateIds;
    const bool datOk =
        LoadActiveVariants(DatFilePath(), &m_activeVariants, &duplicateIds);

    PopulateTree();

    int32_t variantCount = 0;
    for (auto it = m_projects.constBegin();
         it != m_projects.constEnd(); ++it) {
        variantCount += static_cast<int32_t>(it.value().variantFolders.size());
    }

    QString status = Tr(QStringLiteral("%1 project(s), %2 variant(s)."))
                         .arg(m_projects.size())
                         .arg(variantCount);
    if (!datOk) {
        status += Tr(QStringLiteral("  ProjectDefinition.dat not found."));
    }
    if (!duplicateIds.isEmpty()) {
        status += Tr(QStringLiteral("  Duplicate ids: "))
                  + duplicateIds.join(QStringLiteral(", "));
    }
    SetStatus(status);
}

bool MainWindow::ScanProjects(const QString& baseDir,
                              QMap<QString, ProjectInfo>* p_projects) const
{
    if (p_projects == nullptr) {
        return false;
    }
    p_projects->clear();

    QDir dir(baseDir);
    if (!dir.exists()) {
        return false;
    }

    const QStringList entries =
        dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString& entry : entries) {
        const QString projectId = ParseProjectId(entry);
        if (projectId.isEmpty()) {
            continue;
        }

        ProjectInfo& info = (*p_projects)[projectId];
        info.projectId = projectId;
        info.variantFolders.append(entry);
    }
    return true;
}

bool MainWindow::LoadActiveVariants(const QString& datPath,
                                    QMap<QString, QString>* p_active,
                                    QStringList* p_duplicateIds) const
{
    if (p_active == nullptr || p_duplicateIds == nullptr) {
        return false;
    }
    p_active->clear();
    p_duplicateIds->clear();

    QFile file(datPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    const QStringList lines = content.split('\n');
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const int32_t commaPos = static_cast<int32_t>(line.indexOf(','));
        if (commaPos <= 0) {
            continue;
        }

        const QString id     = line.left(commaPos).trimmed();
        const QString folder = line.mid(commaPos + 1).trimmed();
        if (id.isEmpty()) {
            continue;
        }

        if (p_active->contains(id)) {
            if (!p_duplicateIds->contains(id)) {
                p_duplicateIds->append(id);
            }
            continue;   // Keep the first occurrence as the active one.
        }
        p_active->insert(id, folder);
    }
    return true;
}

void MainWindow::PopulateTree()
{
    // Remember which project groups are currently expanded so that a Reload
    // only refreshes the contents and keeps the user's expand/collapse state
    // (default on first load: all collapsed).
    QSet<QString> expandedIds;
    for (int32_t i = 0; i < m_pTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* p_existing = m_pTree->topLevelItem(i);
        if (p_existing != nullptr && p_existing->isExpanded()) {
            expandedIds.insert(p_existing->text(0));
        }
    }

    m_pTree->clear();

    for (auto it = m_projects.constBegin();
         it != m_projects.constEnd(); ++it) {
        const ProjectInfo& info = it.value();

        QTreeWidgetItem* p_top = new QTreeWidgetItem(m_pTree);
        p_top->setText(0, info.projectId);
        // Group rows are headers, not selectable targets.
        p_top->setFlags(Qt::ItemIsEnabled);
        QFont topFont = p_top->font(0);
        topFont.setBold(true);
        p_top->setFont(0, topFont);

        const QString activeFolder = m_activeVariants.value(info.projectId);
        bool activeMatched = false;

        for (const QString& folder : info.variantFolders) {
            QTreeWidgetItem* p_child = new QTreeWidgetItem(p_top);
            p_child->setText(0, folder);
            p_child->setData(0, ROLE_FOLDER, folder);
            p_child->setData(0, ROLE_PROJECT, info.projectId);

            if (folder == activeFolder) {
                p_child->setText(1, Tr(QStringLiteral("Active")));
                QFont f = p_child->font(0);
                f.setBold(true);
                p_child->setFont(0, f);
                p_child->setFont(1, f);
                activeMatched = true;
            }
        }

        if (!activeFolder.isEmpty() && !activeMatched) {
            p_top->setText(1, Tr(QStringLiteral("missing")));
        }

        // Restore previous expand state; new/unseen projects stay collapsed.
        p_top->setExpanded(expandedIds.contains(info.projectId));
    }
}

void MainWindow::SelectVariant(const QString& folder)
{
    if (folder.isEmpty()) {
        return;
    }

    QTreeWidgetItemIterator it(m_pTree);
    while (*it != nullptr) {
        QTreeWidgetItem* p_item = *it;
        if (p_item->data(0, ROLE_FOLDER).toString() == folder) {
            m_pTree->setCurrentItem(p_item);
            m_pTree->scrollToItem(p_item);
            break;
        }
        ++it;
    }
}

void MainWindow::OnTreeActivated(QTreeWidgetItem* p_item, int column)
{
    Q_UNUSED(column);
    if (p_item == nullptr) {
        return;
    }
    // Only variant rows carry a folder; ignore group headers.
    if (p_item->data(0, ROLE_FOLDER).toString().isEmpty()) {
        return;
    }
    OnSetActive();
}

void MainWindow::OnCurrentItemChanged(QTreeWidgetItem* p_current,
                                      QTreeWidgetItem* p_previous)
{
    Q_UNUSED(p_previous);
    if (p_current == nullptr) {
        return;
    }

    const QString folder    = p_current->data(0, ROLE_FOLDER).toString();
    const QString projectId = p_current->data(0, ROLE_PROJECT).toString();
    if (folder.isEmpty() || projectId.isEmpty()) {
        // A project header is selected, not a variant.
        SetStatus(Tr(QStringLiteral("Select a variant under a project.")));
        return;
    }

    SetStatus(Tr(QStringLiteral("Target: project %1 -> %2  "
                                "(click \"Set as active\")"))
                  .arg(projectId, folder));
}

void MainWindow::OnSetActive()
{
    QTreeWidgetItem* p_item = m_pTree->currentItem();
    if (p_item == nullptr) {
        SetStatus(Tr(QStringLiteral("Select a variant first.")));
        return;
    }

    const QString folder    = p_item->data(0, ROLE_FOLDER).toString();
    const QString projectId = p_item->data(0, ROLE_PROJECT).toString();
    if (folder.isEmpty() || projectId.isEmpty()) {
        SetStatus(Tr(QStringLiteral("Select a variant row, not a project.")));
        return;
    }

    // Block editing while that project's program is running.
    if (IsProjectRunning(projectId)) {
        QMessageBox::warning(
            this, Tr(QStringLiteral("Program is running")),
            Tr(QStringLiteral(
                   "\"i-Novatrol %1\" is currently running.\n"
                   "Please close that program before changing project %1."))
                .arg(projectId));
        SetStatus(Tr(QStringLiteral("Cannot change %1: program is running."))
                      .arg(projectId));
        return;
    }

    QMap<QString, QString> selections;
    selections.insert(projectId, folder);

    QString message;
    const bool ok = WriteDefinitions(DatFilePath(), selections, &message);
    if (!ok) {
        QMessageBox::warning(
            this, Tr(QStringLiteral("Update failed")), message);
        SetStatus(message);
        return;
    }

    // Refresh, then keep the user on the row they just activated.
    OnReload();
    SelectVariant(folder);
    SetStatus(Tr(QStringLiteral("Set %1 -> %2   (%3)"))
                  .arg(projectId, folder, message));

    // Verify the license of the project we just switched.
    if (m_licenseProjects.contains(projectId)) {
        RunLicenseCheck(projectId);
    }
}

bool MainWindow::IsProjectRunning(const QString& projectId) const
{
    if (projectId.isEmpty()) {
        return false;
    }

    // The executable is named "i-Novatrol <projectId>.exe". Match the
    // " <projectId>.exe" tail (leading space + .exe) so that, e.g., F30
    // never matches F306.
    const QString needle =
        QStringLiteral(" %1.exe").arg(projectId).toLower();

#ifdef Q_OS_WIN
    // In-process snapshot of running processes - fast (milliseconds).
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;   // Cannot determine - do not block the user.
    }

    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);

    bool found = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            const QString name =
                QString::fromWCharArray(entry.szExeFile).toLower();
            if (name.contains(QStringLiteral("novatrol"))
                && name.contains(needle)) {
                found = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return found;
#else
    return false;
#endif
}

bool MainWindow::WriteDefinitions(const QString& datPath,
                                  const QMap<QString, QString>& selections,
                                  QString* p_message) const
{
    if (p_message == nullptr) {
        return false;
    }

    QFile file(datPath);
    if (!file.exists()) {
        *p_message = Tr(QStringLiteral("File not found: ")) + datPath;
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        *p_message = Tr(QStringLiteral("Cannot open for reading: ")) + datPath;
        return false;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Preserve the original line ending style.
    const QString eol =
        content.contains(QStringLiteral("\r\n"))
            ? QStringLiteral("\r\n")
            : QStringLiteral("\n");

    // Make a backup before touching the original file.
    const QString backupPath = datPath + QStringLiteral(".bak");
    QFile::remove(backupPath);
    QFile::copy(datPath, backupPath);

    const QStringList lines = content.split(eol);
    QStringList outLines;
    outLines.reserve(lines.size());

    QStringList updatedIds;
    QStringList duplicateIds;
    QMap<QString, bool> doneIds;

    for (const QString& line : lines) {
        const int32_t commaPos = static_cast<int32_t>(line.indexOf(','));
        if (commaPos <= 0) {
            outLines.append(line);
            continue;
        }

        const QString id = line.left(commaPos).trimmed();
        if (!selections.contains(id)) {
            outLines.append(line);
            continue;
        }

        if (doneIds.value(id, false)) {
            // A second line with the same id - leave it untouched.
            if (!duplicateIds.contains(id)) {
                duplicateIds.append(id);
            }
            outLines.append(line);
            continue;
        }

        const QString newFolder = selections.value(id);
        outLines.append(id + QStringLiteral(",") + newFolder);
        doneIds.insert(id, true);
        updatedIds.append(id);
    }

    // Report any selected project that had no matching line.
    QStringList notFound;
    for (auto it = selections.constBegin();
         it != selections.constEnd(); ++it) {
        if (!doneIds.value(it.key(), false)) {
            notFound.append(it.key());
        }
    }

    QFile outFile(datPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *p_message = Tr(QStringLiteral("Cannot open for writing: ")) + datPath;
        return false;
    }
    outFile.write(outLines.join(eol).toUtf8());
    outFile.close();

    QString msg =
        Tr(QStringLiteral("Updated %1 line(s). Backup: %2"))
            .arg(updatedIds.size())
            .arg(QFileInfo(backupPath).fileName());
    if (!duplicateIds.isEmpty()) {
        msg += Tr(QStringLiteral(" | Duplicate ids left untouched: "))
               + duplicateIds.join(QStringLiteral(", "));
    }
    if (!notFound.isEmpty()) {
        msg += Tr(QStringLiteral(" | No matching line for: "))
               + notFound.join(QStringLiteral(", "));
    }
    *p_message = msg;
    return true;
}

}  // namespace prjchoosetool
