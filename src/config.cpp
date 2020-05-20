#include "config.h"
#include "UI/ui_ConfigUI.h"



config::config(const QString& configLocation, QWidget* parent /* = nullptr */) : QDialog(parent),
	m_ui(std::make_unique<Ui::configui>()),
	m_settings(std::make_unique<QSettings>(configLocation + "/QtGuiDemo.ini", QSettings::IniFormat, this))
{
	m_ui->setupUi(this);

	// Connect UI Elements.
	connect(m_ui->pbOk, &QPushButton::clicked, this, &config::saveSettings);
	connect(m_ui->pbCancel, &QPushButton::clicked, this, &QWidget::close);
}

config::~config() {
	m_settings->sync();
}


void config::setConfigOption(const QString& option, const QVariant& value) {
	m_settings->setValue(option, value);
}

QVariant config::getConfigOption(const QString& option) const {
	return m_settings->value(option);
}

void config::showEvent(QShowEvent* /* e */) {
	loadSettings();
}

void config::saveSettings() {
	setConfigOption("text", m_ui->ServerAddress->toPlainText());
	setConfigOption("text", m_ui->ServerPort->toPlainText());
	close();
}

void config::loadSettings() {
	m_ui->ServerAddress->setPlainText(getConfigOption("text").toString());
	m_ui->ServerPort->setPlainText(getConfigOption("text").toString());
}