/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Assistant of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qt4projectmanager/qt4projectmanagerconstants.h>

#include "maemodeviceconfigurations.h"
#include "maemosshthread.h"

#include "ui_maemosettingswidget.h"
#include "maemosettingspage.h"

#include <QtCore/QRegExp>
#include <QtGui/QIntValidator>

#include <algorithm>

namespace Qt4ProjectManager {
namespace Internal {

#define PAGE_ID "ZZ.Maemo Device Configurations"
#define PAGE_ID_TR "Maemo Device Configurations"

bool configNameExists(const QList<MaemoDeviceConfig> &devConfs,
                      const QString &name)
{
    return std::find_if(devConfs.constBegin(), devConfs.constEnd(),
        DevConfNameMatcher(name)) != devConfs.constEnd();
}

class PortAndTimeoutValidator : public QIntValidator
{
public:
    PortAndTimeoutValidator() : QIntValidator(0, SHRT_MAX, 0)
    {
    }

    void setValue(int oldValue) { m_oldValue = oldValue; }

    virtual void fixup(QString &input) const
    {
        int dummy = 0;
        if (validate(input, dummy) != Acceptable)
            input = QString::number(m_oldValue);
    }

private:
    int m_oldValue;
};

class NameValidator : public QValidator
{
public:
    NameValidator(const QList<MaemoDeviceConfig> &devConfs)
        : m_devConfs(devConfs)
    {
    }

    void setName(const QString &name) { m_oldName = name; }

    virtual State validate(QString &input, int & /* pos */) const
    {
        if (input.trimmed().isEmpty()
            || (input != m_oldName && configNameExists(m_devConfs, input)))
            return Intermediate;
        return Acceptable;
    }

    virtual void fixup(QString &input) const
    {
        int dummy = 0;
        if (validate(input, dummy) != Acceptable)
            input = m_oldName;
    }

private:
    QString m_oldName;
    const QList<MaemoDeviceConfig> &m_devConfs;
};


class MaemoSettingsWidget : public QWidget
{
    Q_OBJECT
public:
    MaemoSettingsWidget(QWidget *parent);
    ~MaemoSettingsWidget();
    void saveSettings();
private slots:
    void selectionChanged();
    void addConfig();
    void deleteConfig();
    void configNameEditingFinished();
    void deviceTypeChanged();
    void authenticationTypeChanged();
    void hostNameEditingFinished();
    void portEditingFinished();
    void timeoutEditingFinished();
    void userNameEditingFinished();
    void passwordEditingFinished();
    void keyFileEditingFinished();
    void testConfig();
    void enableTestStop();
    void processSshOutput(const QString &data);
    void handleSshFinished();
    void stopConfigTest();

private:
    void initGui();
    void display(const MaemoDeviceConfig &devConfig);
    MaemoDeviceConfig &currentConfig();
    void setPortOrTimeout(const QLineEdit *lineEdit, int &confVal,
                          PortAndTimeoutValidator &validator);
    void clearDetails();
    QString parseTestOutput();

    Ui_maemoSettingsWidget *m_ui;
    QList<MaemoDeviceConfig> m_devConfs;
    PortAndTimeoutValidator m_portValidator;
    PortAndTimeoutValidator m_timeoutValidator;
    NameValidator m_nameValidator;
#ifdef USE_SSH_LIB
    MaemoSshRunner *m_deviceTester;
#endif
    QString m_deviceTestOutput;
    QString m_defaultTestOutput;
};

MaemoSettingsPage::MaemoSettingsPage(QObject *parent)
    : Core::IOptionsPage(parent)
{
}

MaemoSettingsPage::~MaemoSettingsPage()
{
}

QString MaemoSettingsPage::id() const
{
    return QLatin1String(PAGE_ID);
}

QString MaemoSettingsPage::trName() const
{
    return tr(PAGE_ID_TR);
}

QString MaemoSettingsPage::category() const
{
    return QLatin1String(Constants::QT_SETTINGS_CATEGORY);
}

QString MaemoSettingsPage::trCategory() const
{
    return QCoreApplication::translate("Qt4ProjectManager",
        Constants::QT_SETTINGS_CATEGORY);
}

QWidget *MaemoSettingsPage::createPage(QWidget *parent)
{
    m_widget = new MaemoSettingsWidget(parent);
    return m_widget;
}

void MaemoSettingsPage::apply()
{
    m_widget->saveSettings();
}

void MaemoSettingsPage::finish()
{
}

MaemoSettingsWidget::MaemoSettingsWidget(QWidget *parent)
    : QWidget(parent),
      m_ui(new Ui_maemoSettingsWidget),
      m_devConfs(MaemoDeviceConfigurations::instance().devConfigs()),
      m_nameValidator(m_devConfs)
#ifdef USE_SSH_LIB
      , m_deviceTester(0)
#endif

{
    initGui();
}

MaemoSettingsWidget::~MaemoSettingsWidget()
{
}

void MaemoSettingsWidget::initGui()
{
    m_ui->setupUi(this);
    m_ui->nameLineEdit->setValidator(&m_nameValidator);
    m_ui->portLineEdit->setValidator(&m_portValidator);
    m_ui->timeoutLineEdit->setValidator(&m_timeoutValidator);
    m_ui->keyFileLineEdit->setExpectedKind(Utils::PathChooser::File);
    foreach(const MaemoDeviceConfig &devConf, m_devConfs)
        m_ui->configListWidget->addItem(devConf.name);
    m_defaultTestOutput = m_ui->testResultEdit->toPlainText();

#ifndef USE_SSH_LIB // Password authentication does not currently work due to ssh/scp issues.
    m_ui->testConfigButton->hide();
    m_ui->testResultEdit->hide();
    m_ui->authTypeLabel->hide();
    m_ui->authTypeButtonsWidget->hide();
    m_ui->passwordLabel->hide();
    m_ui->pwdLineEdit->hide();
#endif
}

void MaemoSettingsWidget::addConfig()
{
    QLatin1String prefix("New Device Configuration ");
    int suffix = 1;
    QString newName;
    bool isUnique = false;
    do {
        newName = prefix + QString::number(suffix++);
        isUnique = !configNameExists(m_devConfs, newName);
    } while (!isUnique);

    m_devConfs.append(MaemoDeviceConfig(newName));
    m_ui->configListWidget->addItem(newName);
    m_ui->configListWidget->setCurrentRow(m_ui->configListWidget->count() - 1);
    m_ui->nameLineEdit->selectAll();
    m_ui->removeConfigButton->setEnabled(true);
    m_ui->nameLineEdit->setFocus();
}

void MaemoSettingsWidget::deleteConfig()
{
    const QList<QListWidgetItem *> &selectedItems =
        m_ui->configListWidget->selectedItems();
    if (selectedItems.isEmpty())
        return;
    QListWidgetItem *item = selectedItems.first();
    const int selectedRow = m_ui->configListWidget->row(item);
    m_devConfs.removeAt(selectedRow);
    disconnect(m_ui->configListWidget, SIGNAL(itemSelectionChanged()), 0, 0);
    delete m_ui->configListWidget->takeItem(selectedRow);
    connect(m_ui->configListWidget, SIGNAL(itemSelectionChanged()),
            this, SLOT(selectionChanged()));
    Q_ASSERT(m_ui->configListWidget->count() == m_devConfs.count());
    selectionChanged();
}

void MaemoSettingsWidget::display(const MaemoDeviceConfig &devConfig)
{
    m_ui->nameLineEdit->setText(devConfig.name);
    if (devConfig.type == MaemoDeviceConfig::Physical)
        m_ui->deviceButton->setChecked(true);
    else
        m_ui->simulatorButton->setChecked(true);
    if (devConfig.authentication == MaemoDeviceConfig::Password)
        m_ui->passwordButton->setChecked(true);
    else
        m_ui->keyButton->setChecked(true);
    m_ui->hostLineEdit->setText(devConfig.host);
    m_ui->portLineEdit->setText(QString::number(devConfig.port));
    m_ui->timeoutLineEdit->setText(QString::number(devConfig.timeout));
    m_ui->userLineEdit->setText(devConfig.uname);
    m_ui->pwdLineEdit->setText(devConfig.pwd);
    m_ui->keyFileLineEdit->setPath(devConfig.keyFile);
    m_ui->detailsWidget->setEnabled(true);
    m_nameValidator.setName(devConfig.name);
    m_portValidator.setValue(devConfig.port);
    m_timeoutValidator.setValue(devConfig.timeout);
    m_ui->detailsWidget->setEnabled(true);
}

void MaemoSettingsWidget::saveSettings()
{
    MaemoDeviceConfigurations::instance().setDevConfigs(m_devConfs);
}

MaemoDeviceConfig &MaemoSettingsWidget::currentConfig()
{
    Q_ASSERT(m_ui->configListWidget->count() == m_devConfs.count());
    const QList<QListWidgetItem *> &selectedItems =
        m_ui->configListWidget->selectedItems();
    Q_ASSERT(selectedItems.count() == 1);
    const int selectedRow = m_ui->configListWidget->row(selectedItems.first());
    Q_ASSERT(selectedRow < m_devConfs.count());
    return m_devConfs[selectedRow];
}

void MaemoSettingsWidget::configNameEditingFinished()
{
    const QString &newName = m_ui->nameLineEdit->text();
    currentConfig().name = newName;
    m_nameValidator.setName(newName);
    m_ui->configListWidget->currentItem()->setText(newName);
}

void MaemoSettingsWidget::deviceTypeChanged()
{
    currentConfig().type =
        m_ui->deviceButton->isChecked()
            ? MaemoDeviceConfig::Physical
            : MaemoDeviceConfig::Simulator;
}

void MaemoSettingsWidget::authenticationTypeChanged()
{
    const bool usePassword = m_ui->passwordButton->isChecked();
    currentConfig().authentication = usePassword
        ? MaemoDeviceConfig::Password
        : MaemoDeviceConfig::Key;
    m_ui->pwdLineEdit->setEnabled(usePassword);
    m_ui->passwordLabel->setEnabled(usePassword);
    m_ui->keyFileLineEdit->setEnabled(!usePassword);
    m_ui->keyLabel->setEnabled(!usePassword);
}

void MaemoSettingsWidget::hostNameEditingFinished()
{
    currentConfig().host = m_ui->hostLineEdit->text();
}

void MaemoSettingsWidget::portEditingFinished()
{
    setPortOrTimeout(m_ui->portLineEdit, currentConfig().port, m_portValidator);
}

void MaemoSettingsWidget::timeoutEditingFinished()
{
    setPortOrTimeout(m_ui->timeoutLineEdit, currentConfig().timeout,
                     m_timeoutValidator);
}

void MaemoSettingsWidget::setPortOrTimeout(const QLineEdit *lineEdit,
    int &confVal, PortAndTimeoutValidator &validator)
{
    bool ok;
    confVal = lineEdit->text().toInt(&ok);
    Q_ASSERT(ok);
    validator.setValue(confVal);
}

void MaemoSettingsWidget::userNameEditingFinished()
{
    currentConfig().uname = m_ui->userLineEdit->text();
}

void MaemoSettingsWidget::passwordEditingFinished()
{
    currentConfig().pwd = m_ui->pwdLineEdit->text();
}

void MaemoSettingsWidget::keyFileEditingFinished()
{
    currentConfig().keyFile = m_ui->keyFileLineEdit->path();
}

void MaemoSettingsWidget::testConfig()
{
#ifdef USE_SSH_LIB
    qDebug("Oh yes, this config will be tested!");
    if (m_deviceTester)
        return;

    m_ui->testResultEdit->setPlainText(m_defaultTestOutput);
    QLatin1String sysInfoCmd("uname -rsm");
    QLatin1String qtInfoCmd("dpkg -l |grep libqt "
        "|sed 's/[[:space:]][[:space:]]*/ /g' "
        "|cut -d ' ' -f 2,3 |sed 's/~.*//g'");
    QString command(sysInfoCmd + " && " + qtInfoCmd);
    m_deviceTester = new MaemoSshRunner(currentConfig(), command);
    connect(m_deviceTester, SIGNAL(connectionEstablished()),
            this, SLOT(enableTestStop()));
    connect(m_deviceTester, SIGNAL(remoteOutput(QString)),
            this, SLOT(processSshOutput(QString)));
    connect(m_deviceTester, SIGNAL(finished()),
            this, SLOT(handleSshFinished()));
    m_deviceTester->start();
#endif
}

void MaemoSettingsWidget::enableTestStop()
{
    m_ui->testConfigButton->disconnect();
    m_ui->testConfigButton->setText(tr("Stop test"));
    connect(m_ui->testConfigButton, SIGNAL(clicked()),
            this, SLOT(stopConfigTest()));
}

void MaemoSettingsWidget::processSshOutput(const QString &data)
{
    qDebug("%s", qPrintable(data));
    m_deviceTestOutput.append(data);
}

void MaemoSettingsWidget::handleSshFinished()
{
#ifdef USE_SSH_LIB
    qDebug("================> %s", Q_FUNC_INFO);
    if (!m_deviceTester)
        return;

    QString output;
    if (m_deviceTester->hasError()) {
        output = tr("Device configuration test failed:\n");
        output.append(m_deviceTester->error());
    } else {
        output = parseTestOutput();
    }
    m_ui->testResultEdit->setPlainText(output);
    stopConfigTest();
#endif
}

void MaemoSettingsWidget::stopConfigTest()
{
#ifdef USE_SSH_LIB
    qDebug("================> %s", Q_FUNC_INFO);
    if (m_deviceTester) {
        qDebug("Actually doing something");
        m_deviceTester->disconnect();
        const bool buttonWasEnabled = m_ui->testConfigButton->isEnabled();
        m_ui->testConfigButton->setEnabled(false);
        m_deviceTester->stop();
        delete m_deviceTester;
        m_deviceTester = 0;
        m_deviceTestOutput.clear();
        m_ui->testConfigButton->setText(tr("Test"));
        m_ui->testConfigButton->disconnect();
        connect(m_ui->testConfigButton, SIGNAL(clicked()),
                this, SLOT(testConfig()));
        m_ui->testConfigButton->setEnabled(buttonWasEnabled);
    }
#endif
}

QString MaemoSettingsWidget::parseTestOutput()
{
    QString output;
    const QRegExp unamePattern(QLatin1String("Linux (\\S+)\\s(\\S+)"));
    int index = unamePattern.indexIn(m_deviceTestOutput);
    if (index == -1) {
        output = tr("Device configuration test failed: Unexpected output:\n");
        output.append(m_deviceTestOutput);
        return output;
    }

    output = tr("Hardware architecture: %1\n").arg(unamePattern.cap(2));
    output.append(tr("Kernel version: %1\n").arg(unamePattern.cap(1)));
    output.prepend(tr("Device configuration successful.\n"));
    const QRegExp dkpgPattern(QLatin1String("libqt\\S+ \\d\\.\\d\\.\\d"));
    index = dkpgPattern.indexIn(m_deviceTestOutput);
    if (index == -1) {
        output.append("No Qt packages installed.");
        return output;
    }
    output.append("List of installed Qt packages:\n");
    do {
        output.append(QLatin1String("\t") + dkpgPattern.cap(0)
                      + QLatin1String("\n"));
        index = dkpgPattern.indexIn(m_deviceTestOutput, index + 1);
    } while (index != -1);
    return output;
}

void MaemoSettingsWidget::selectionChanged()
{
    const QList<QListWidgetItem *> &selectedItems =
        m_ui->configListWidget->selectedItems();
    Q_ASSERT(selectedItems.count() <= 1);
    stopConfigTest();
    m_ui->testResultEdit->setPlainText(m_defaultTestOutput);
    if (selectedItems.isEmpty()) {
        m_ui->removeConfigButton->setEnabled(false);
        m_ui->testConfigButton->setEnabled(false);
        clearDetails();
        m_ui->detailsWidget->setEnabled(false);
    } else {
        m_ui->removeConfigButton->setEnabled(true);
        m_ui->testConfigButton->setEnabled(true);
        display(currentConfig());
    }
}

void MaemoSettingsWidget::clearDetails()
{
    m_ui->nameLineEdit->clear();
    m_ui->hostLineEdit->clear();
    m_ui->portLineEdit->clear();
    m_ui->timeoutLineEdit->clear();
    m_ui->userLineEdit->clear();
    m_ui->pwdLineEdit->clear();
}

} // namespace Internal
} // namespace Qt4ProjectManager

#include "maemosettingspage.moc"
