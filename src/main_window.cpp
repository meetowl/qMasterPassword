/*
 * Copyright (C) 2014 Beat Küng <beat-kueng@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "main_window.h"
#include "ui_main_window.h"
#include "add_user.h"
#include "edit_site_widget.h"
#include "logging.h"
#include "pushbutton_delegate.h"

#include <iostream>
using namespace std;

#include <QMessageBox>
#include <QSettings>
#include <QDataStream>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QClipboard>

MainWindow::MainWindow(QWidget *parent) :
		QMainWindow(parent), m_ui(new Ui::MainWindow) {
	m_ui->setupUi(this);

	initSitesView();
	readSettings();
	m_ui->btnDeleteUser->setEnabled(m_ui->cmbUserName->count() > 0);
	enableUI(false);
	setWindowIcon(QIcon(":/app_icon.png"));
}

MainWindow::~MainWindow() {
	delete m_ui;
	delete m_sites_model;
}

void MainWindow::initSitesView() {
	m_sites_model = new QStandardItemModel();
	m_sites_model->setColumnCount(7);
	int i = 0;
	m_sites_model->setHeaderData(i++, Qt::Horizontal, QObject::tr("Site"));
	m_sites_model->setHeaderData(i++, Qt::Horizontal, QObject::tr("Login Name"));
	m_sites_model->setHeaderData(i++, Qt::Horizontal, QObject::tr("Password"));
	m_sites_model->setHeaderData(m_copy_column_idx = i++, Qt::Horizontal,
			QObject::tr("")); //copy pw button
	m_sites_model->setHeaderData(i++, Qt::Horizontal, QObject::tr("Comment"));
	m_sites_model->setHeaderData(i++, Qt::Horizontal, QObject::tr("Type"));
	m_sites_model->setHeaderData(i++, Qt::Horizontal, QObject::tr("Counter"));

	m_proxy_model = new MySortFilterProxyModel(*this);
	m_proxy_model->setSourceModel(m_sites_model);
	m_ui->tblSites->setModel(m_proxy_model);

	PushButtonDelegate* button_item_delegate = new PushButtonDelegate(*this);
	m_ui->tblSites->setItemDelegateForColumn(m_copy_column_idx, button_item_delegate);

	m_ui->tblSites->horizontalHeader()->setSectionResizeMode(m_copy_column_idx,
		QHeaderView::Fixed);

    connect(m_ui->tblSites->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this,
            SLOT(selectionChanged(const QItemSelection&, const QItemSelection&)));
}

void MainWindow::addSiteToUI(UiSite& site) {
	QList<QStandardItem*> items;
	for (int i = 0; i < 7; ++i) {
		QStandardItem* item = new TableItem(site, "");
		item->setEditable(false);
		item->setDragEnabled(false);
		item->setDropEnabled(false);
		items.push_back(item);
	}
	m_sites_model->appendRow(items);
	updateModelItem(m_sites_model->rowCount()-1, site);

	/*
	//simpler version to display a 'copy to clipboard' button
	// (but it will be hidden after filter changes)
	QPushButton* btn = new UserPushButton(site, "");
	btn->setIcon(QIcon(":/copy.png"));
	btn->setFixedWidth(btn->sizeHint().width());
	connect(btn, SIGNAL(clicked()), this, SLOT(copyPWToClipboardClicked()));
	m_ui->tblSites->setIndexWidget(m_proxy_model->mapFromSource(
		m_sites_model->index(m_sites_model->rowCount() - 1, m_copy_column_idx)), btn);
	*/

	uiSitesTableChanged();
}
void MainWindow::updateModelItem(int row, const UiSite& site) {
	int i = 0;
	m_sites_model->item(row, i++)->setText(QString::fromStdString(site.site.getName()));
	m_sites_model->item(row, i++)->setText(site.user_name);
	if (site.password_visible) {
		string password = m_master_password.sitePassword(site.site);
		m_sites_model->item(row, i++)->setText(QString::fromStdString(password));
	} else {
		m_sites_model->item(row, i++)->setText("***");
	}
	++i; //copy button
	m_sites_model->item(row, i++)->setText(site.comment);
	m_sites_model->item(row, i++)->setText(QString::fromStdString(
			MPSiteTypeToString(site.site.getType())));
	m_sites_model->item(row, i++)->setText(QString::number(site.site.getCounter()));
}

void MainWindow::clearSitesUI() {
	m_sites_model->removeRows(0, m_sites_model->rowCount());
	m_ui->btnDeleteSite->setEnabled(false);
	m_ui->btnEditSite->setEnabled(false);
}

void MainWindow::loginLogoutClicked() {
	if (m_master_password.isLoggedIn())
		logout();
	else
		login();
}

void MainWindow::login() {
	QString user_name = m_ui->cmbUserName->currentText();
	QString password = m_ui->txtPassword->text();
	auto iter_user = m_users.find(user_name);
	if (user_name.length() == 0 || password.length() == 0
			|| iter_user == m_users.end())
		return;

	try {
		m_current_user = &iter_user.value();
		m_master_password.logout();
		m_master_password.login(user_name.toStdString(), password.toStdString());
		m_ui->btnLoginLogout->setText(tr("Logout"));
		enableUI(true);
		clearSitesUI();
		for (auto& site : m_current_user->getSites()) {
			addSiteToUI(*site);
		}

		//clear the password: no need to store it anymore
		m_ui->txtPassword->setText("");
		m_ui->tblSites->setFocus();

	} catch(CryptoException& e) {
		QString error_msg;
		switch(e.type) {
		case CryptoException::Type_HMAC_SHA256_failed:
			error_msg = tr("HMAC SHA256 function failed");
			break;
		case CryptoException::Type_scrypt_failed:
			error_msg = tr("scrypt function failed");
			break;
		case CryptoException::Type_not_logged_in:
			error_msg = tr("not logged in");
			break;
		}
		QMessageBox::warning(this, tr("Cryptographic exception"),
			tr("Error: %1.\n"
			"Should it happen again, then something is seriously wrong with the program installation.").arg(error_msg));
	}
}

void MainWindow::logout() {
	m_master_password.logout();
	m_current_user = nullptr;
	m_ui->btnLoginLogout->setText(tr("Login"));
	m_ui->txtPassword->setText("");
	enableUI(false);
	clearSitesUI();
}

void MainWindow::enableUI(bool logged_in) {
	m_ui->cmbUserName->setEnabled(!logged_in);
	m_ui->txtPassword->setEnabled(!logged_in);
	m_ui->btnAddSite->setEnabled(logged_in);
}

void MainWindow::addUser() {
	AddUser add_user(this);
	if (add_user.exec() == 1) { //accepted
		QString user_name = add_user.userName();
		if (m_users.find(user_name) != m_users.end()) {
			QMessageBox::warning(this, tr("User exists"),
					tr("Error: user already exists."));
			return;
		}
		UiUser user(user_name);
		m_users.insert(user_name, user);
		m_ui->txtPassword->setText(add_user.password());
		m_ui->cmbUserName->addItem(user_name);
		m_ui->cmbUserName->setCurrentIndex(m_ui->cmbUserName->count() - 1);
		m_ui->btnDeleteUser->setEnabled(true);
		login();
	}
}

void MainWindow::deleteUser() {
	int selected = m_ui->cmbUserName->currentIndex();
	if (selected >= 0) {
		QString user_name = m_ui->cmbUserName->itemText(selected);
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, tr("Delete User"),
				tr("Do you really want to delete the user %1?").arg(user_name),
				QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			logout();
			m_ui->cmbUserName->removeItem(selected);
			m_users.remove(user_name);
		}
	}
	if (m_ui->cmbUserName->count() == 0)
		m_ui->btnDeleteUser->setEnabled(false);
}

void MainWindow::closeEvent(QCloseEvent *event) {
	saveSettings();
    QMainWindow::closeEvent(event);
}
void MainWindow::saveSettings() {
	QSettings settings("qMasterPassword", "qMasterPassword");
	settings.setValue("main_window/geometry", saveGeometry());
	settings.setValue("main_window/window_state", saveState());
	settings.setValue("main_window/selected_user",
			m_ui->cmbUserName->currentText());
	settings.setValue("main_window/table_header_state",
			m_ui->tblSites->horizontalHeader()->saveState());

	QByteArray categories_data;
	QDataStream categories_stream(&categories_data, QIODevice::ReadWrite);
	categories_stream << m_categories;
	settings.setValue("data/categories", categories_data);

	QByteArray user_data;
	QDataStream stream(&user_data, QIODevice::ReadWrite);
	stream << m_users;
	settings.setValue("data/users", user_data);
}
void MainWindow::readSettings() {
	QSettings settings("qMasterPassword", "qMasterPassword");
	restoreGeometry(settings.value("main_window/geometry").toByteArray());
	restoreState(settings.value("main_window/window_state").toByteArray());
	m_ui->tblSites->horizontalHeader()->restoreState(
			settings.value("main_window/table_header_state").toByteArray());

    QDataStream categories_stream(settings.value("data/categories").toByteArray());
    categories_stream >> m_categories;
	if (m_categories.isEmpty()) {
		//add default categories
		m_categories[m_next_category_id++] = tr("All");
		m_categories[m_next_category_id++] = tr("Personal");
		m_categories[m_next_category_id++] = tr("Work");
		m_categories[m_next_category_id++] = tr("eShopping");
		m_categories[m_next_category_id++] = tr("Social Networks");
		m_categories[m_next_category_id++] = tr("Bank");
		m_categories[m_next_category_id++] = tr("Forum");
		m_categories[m_next_category_id++] = tr("eMail");
	} else {
		m_next_category_id = 0;
		for (auto category : m_categories.keys()) {
			if (category > m_next_category_id)
				m_next_category_id = category;
		}
		++m_next_category_id;
		LOG(DEBUG, "next category id: %i", (int )m_next_category_id);
    }
	for (auto iter = m_categories.begin(); iter != m_categories.end(); ++iter) {
		addCategory(iter.value(), iter.key());
	}
	selectCategory(0);

    QDataStream stream(settings.value("data/users").toByteArray());
    QString selected_user = settings.value("main_window/selected_user").toString();
    stream >> m_users;
    int selected_index = -1;
	for (auto& user : m_users) {
		LOG(DEBUG, "Read user: %s (%i sites)",
			user.getUserName().toStdString().c_str(), user.getSites().count());

		m_ui->cmbUserName->addItem(user.getUserName());
		if (user.getUserName() == selected_user)
			selected_index = m_ui->cmbUserName->count() - 1;
    }
	if (selected_index != -1)
		m_ui->cmbUserName->setCurrentIndex(selected_index);
	if (!m_users.isEmpty())
		m_ui->txtPassword->setFocus();
}

void MainWindow::addSite() {
	if (!m_current_user) return;
	UiSite* site = new UiSite();
	EditSiteWidget edit_site(m_categories, *site, EditSiteWidget::Type_new, this);
	if (edit_site.exec() == 1) { //accepted
		edit_site.applyData();
		m_current_user->getSites().append(site);
		addSiteToUI(*site);
	} else {
		delete site;
	}
}

void MainWindow::deleteSite() {
	if (!m_current_user) return;
	TableItem* item = getSelectedItem();
	if (!item) return;
	m_sites_model->removeRow(item->row());
	for (auto iter = m_current_user->getSites().begin();
			iter != m_current_user->getSites().end(); ++iter) {
		if(*iter == &item->site()) {
			delete *iter;
			m_current_user->getSites().erase(iter);
			break;
		}
	}
}

void MainWindow::editSite() {
	if (!m_current_user) return;
	TableItem* item = getSelectedItem();
	if (!item) return;
	UiSite& site = item->site();

	EditSiteWidget edit_site(m_categories, site, EditSiteWidget::Type_edit, this);
	if (edit_site.exec() == 1) { //accepted
		edit_site.applyData();
		updateModelItem(item->row(), site);
	}
}

TableItem* MainWindow::getSelectedItem() {
	QItemSelectionModel* selection = m_ui->tblSites->selectionModel();
	QModelIndexList selected_rows = selection->selectedRows();
	if (selected_rows.count() == 0)
		return nullptr;
	return dynamic_cast<TableItem*>(m_sites_model->itemFromIndex(
			m_proxy_model->mapToSource(selected_rows.at(0))));
}

void MainWindow::selectionChanged(const QItemSelection& selected,
		const QItemSelection& deselected) {
	bool has_selection = selected.count() > 0;
	m_ui->btnEditSite->setEnabled(has_selection);
	m_ui->btnDeleteSite->setEnabled(has_selection);
}

void MainWindow::addCategory(const QString& name, CategoryId id) {
	if (id == -1)
		id = m_next_category_id++;
	m_categories[id] = name;
	QPushButton* button = new CategoryButton(id, name);
	button->setCheckable(true);
    connect(button, SIGNAL(clicked()), this, SLOT(categoryButtonPressed()));
	m_ui->layoutCategories->addWidget(button);
}

void MainWindow::selectCategory(CategoryId category) {
	for (int i = 0; i < m_ui->layoutCategories->count(); ++i) {
		CategoryButton* button = dynamic_cast<CategoryButton*>(
				m_ui->layoutCategories->itemAt(i)->widget());
		if (button) {
			button->setChecked(button->category_id == category);
		}
	}
	m_selected_category = category;
	m_proxy_model->invalidate();
	uiSitesTableChanged();
}

void MainWindow::categoryButtonPressed() {
	CategoryButton* button = dynamic_cast<CategoryButton*>(sender());
	if (!button) return;
	selectCategory(button->category_id);
}

void MainWindow::filterTextChanged(QString filter_text) {
	m_proxy_model->setFilterRegExp(
			QRegExp(filter_text, Qt::CaseInsensitive, QRegExp::FixedString));
	m_proxy_model->setFilterKeyColumn(0);
	uiSitesTableChanged();
}

void MainWindow::copyPWToClipboardClicked() {
	LOG(DEBUG, "copy to clipboard clicked");
	UserPushButton* button = dynamic_cast<UserPushButton*>(sender());
	if (!button) return;
	string password = m_master_password.sitePassword(button->site().site);
	QClipboard *clipboard = QApplication::clipboard();
	QString originalText = clipboard->text();
	clipboard->setText(QString::fromStdString(password));
}
void MainWindow::showHidePWClicked() {
	LOG(DEBUG, "show/hide PW clicked");
	UserPushButton* button = dynamic_cast<UserPushButton*>(sender());
	if (!button) return;
	UiSite& site = button->site();
	site.password_visible = !site.password_visible;
	button->setIcon(QIcon(site.password_visible ? ":/hidden.png" : ":/shown.png"));

	for (int i = 0; i < m_sites_model->rowCount(); ++i) {
		TableItem* item = dynamic_cast<TableItem*>(m_sites_model->item(i, 0));
		if (item && &item->site() == &site) {
			updateModelItem(i, site);
			break;
		}
	}
}

void MainWindow::uiSitesTableChanged() {
	//(re-)open persistent editors, because after refilter, they get hidden.
	//yes it's ugly, but i didn't see an easier way...
	for (int row = 0; row < m_proxy_model->rowCount(); ++row) {
		m_ui->tblSites->openPersistentEditor(
			m_proxy_model->index(row, m_copy_column_idx));
	}
	m_ui->tblSites->resizeColumnToContents(m_copy_column_idx);
}

bool MySortFilterProxyModel::filterAcceptsRow(int source_row,
		const QModelIndex& source_parent) const {
	if (m_main_window.selectedCategory() != 0) {
		QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
		TableItem* item = dynamic_cast<TableItem*>(m_main_window.model()->itemFromIndex(index));
		if (item) {
			if (!item->site().category_ids.contains(m_main_window.selectedCategory()))
				return false;
		}
	}

	return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}
