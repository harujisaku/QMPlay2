/*
	QMPlay2 is a video and audio player.
	Copyright (C) 2010-2017  Błażej Szczygieł

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Lesser General Public License as published
	by the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Radio/RadioBrowserModel.hpp>

#include <NetworkAccess.hpp>
#include <QMPlay2Core.hpp>
#include <Functions.hpp>
#include <Json11.hpp>

#include <QPainter>
#include <QUrl>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	#include <QWindow>
#endif

#include <algorithm>

struct Column
{
	QByteArray url, homePageUrl, id;

	QByteArray iconUrl;
	QPointer<NetworkReply> iconReply;
	QPixmap icon;

	QString name, streamInfo, country, tags;
	qint32 rating;
};

/**/

RadioBrowserModel::RadioBrowserModel(const QWidget *widget) :
	m_widget(widget),
	m_net(new NetworkAccess),
	m_sortColumnIdx(0),
	m_sortOrder(Qt::AscendingOrder)
{
	connect(m_net, SIGNAL(finished(NetworkReply *)), this, SLOT(replyFinished(NetworkReply *)));
}
RadioBrowserModel::~RadioBrowserModel()
{
	delete m_net;
}

void RadioBrowserModel::searchRadios(const QString &text, const QString &searchBy)
{
	const QByteArray postData = searchBy.toLatin1().toLower() + "=" + text.toUtf8().toPercentEncoding();

	for (const QSharedPointer<Column> &column : m_rows)
		delete column->iconReply;
	delete m_replySearch;

	beginResetModel();
	m_rowsToDisplay.clear();
	endResetModel();

	m_rows.clear();

	m_replySearch = m_net->start(QString("%1/stations/search").arg(g_radioBrowserBaseApiUrl), postData, NetworkAccess::UrlEncoded);
}

void RadioBrowserModel::loadIcons(const int first, const int last)
{
	for (int i = first; i <= last; ++i)
	{
		Column *column = m_rowsToDisplay[i].data();
		if (!column->iconReply && !column->iconUrl.isEmpty())
		{
			column->iconReply = m_net->start(column->iconUrl);
			for (QSharedPointer<Column> &c : m_rows)
			{
				if (c == column)
					continue;
				if (c->iconUrl == column->iconUrl)
				{
					c->iconReply = column->iconReply;
					c->iconUrl.clear();
				}
			}
			column->iconUrl.clear();
		}
	}
}

QString RadioBrowserModel::getName(const QModelIndex &index) const
{
	return m_rowsToDisplay.value(index.row())->name;
}
QUrl RadioBrowserModel::getUrl(const QModelIndex &index) const
{
	return QUrl(m_rowsToDisplay.value(index.row())->url);
}
QUrl RadioBrowserModel::getEditUrl(const QModelIndex &index) const
{
	return QUrl("http://www.radio-browser.info/gui/#/edit/" + m_rowsToDisplay.value(index.row())->id);
}
QUrl RadioBrowserModel::getHomePageUrl(const QModelIndex &index) const
{
	return QUrl(m_rowsToDisplay.value(index.row())->homePageUrl);
}

QModelIndex RadioBrowserModel::index(int row, int column, const QModelIndex &parent) const
{
	if (parent.isValid())
		return QModelIndex();
	return createIndex(row, column);

}
QModelIndex RadioBrowserModel::parent(const QModelIndex &child) const
{
	Q_UNUSED(child)
	return QModelIndex();
}
int RadioBrowserModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_rowsToDisplay.size();
}
int RadioBrowserModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return 5;
}
QVariant RadioBrowserModel::data(const QModelIndex &index, int role) const
{
	if (index.isValid())
	{
		const Column *column = m_rowsToDisplay[index.row()].data();
		const int col = index.column();
		switch (role)
		{
			case Qt::DisplayRole:
			{
				switch (col)
				{
					case 0:
						return column->name;
					case 1:
						return column->streamInfo;
					case 2:
						return column->country;
					case 3:
						return column->tags;
					case 4:
						return column->rating;
				}
				break;
			}
			case Qt::DecorationRole:
			{
				if (col == 0)
					return column->icon;
				break;
			}
			case Qt::ToolTipRole:
			{
				if (col == 3)
					return column->tags;
				break;
			}
			case Qt::TextAlignmentRole:
			{
				if (col == 1)
					return Qt::AlignCenter;
				break;
			}
		}
	}
	return QVariant();
}
QVariant RadioBrowserModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
			case 0:
				return tr("Name");
			case 1:
				return tr("Stream info");
			case 2:
				return tr("Country");
			case 3:
				return tr("Tags");
			case 4:
				return tr("Rating");
		}
	}
	return QVariant();
}
Qt::ItemFlags RadioBrowserModel::flags(const QModelIndex &index) const
{
	const Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
	if (index.isValid())
		return Qt::ItemIsDragEnabled | defaultFlags;
	return defaultFlags;
}
void RadioBrowserModel::sort(int columnIdx, Qt::SortOrder order)
{
	const auto sortCallback = [=](const QSharedPointer<Column> &a, const QSharedPointer<Column> &b) {
		QString *str1 = nullptr;
		QString *str2 = nullptr;
		switch (columnIdx)
		{
			case 0:
				str1 = &a->name;
				str2 = &b->name;
				break;
			case 1:
				str1 = &a->streamInfo;
				str2 = &b->streamInfo;
				break;
			case 2:
				str1 = &a->country;
				str2 = &b->country;
				break;
			case 3:
				str1 = &a->tags;
				str2 = &b->tags;
				break;
			case 4:
				switch (order)
				{
					case Qt::AscendingOrder:
						return a->rating > b->rating;
					case Qt::DescendingOrder:
						return a->rating < b->rating;
				}
				break;
		}
		if (str1 && str2)
		{
			switch (order)
			{
				case Qt::AscendingOrder:
					return str1->compare(*str2, Qt::CaseInsensitive) > 0;
				case Qt::DescendingOrder:
					return str2->compare(*str1, Qt::CaseInsensitive) > 0;
			}
		}
		return false;
	};

	bool rowsToDisplayEquals = false;

	beginResetModel();
	if (m_rowsToDisplay.size() == m_rows.size())
	{
		m_rowsToDisplay.clear();
		rowsToDisplayEquals = true;
	}
	std::sort(m_rows.begin(), m_rows.end(), sortCallback);
	if (rowsToDisplayEquals)
		m_rowsToDisplay = m_rows;
	else
		std::sort(m_rowsToDisplay.begin(), m_rowsToDisplay.end(), sortCallback);
	endResetModel();

	m_sortColumnIdx = columnIdx;
	m_sortOrder = order;
}

void RadioBrowserModel::setFiltrText(const QString &text)
{
	const QString textToFilter = text.simplified();
	beginResetModel();
	if (textToFilter.isEmpty())
	{
		m_rowsToDisplay = m_rows;
	}
	else
	{
		m_rowsToDisplay.clear();
		for (const QSharedPointer<Column> &column : m_rows)
		{
			if (column->name.contains(text, Qt::CaseInsensitive))
				m_rowsToDisplay.append(column);
		}
	}
	endResetModel();
}

void RadioBrowserModel::replyFinished(NetworkReply *reply)
{
	if (!reply->hasError())
	{
		if (reply == m_replySearch)
		{
			const Json json = Json::parse(reply->readAll());
			if (!json.is_null() && json.type() == Json::ARRAY)
			{
				const Json::array &arrayItems = json.array_items();
				beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + arrayItems.size() - 1);
				m_rowsToDisplay.clear();

				const QPixmap radioIcon = QIcon(":/radio.svgz").pixmap(elementHeight(), elementHeight());

				for (auto &&item : arrayItems)
				{
					if (item.type() == Json::OBJECT)
					{
						QString streamInfo = item["codec"].string_value();
						if (!streamInfo.isEmpty())
						{
							if (streamInfo.compare("unknown", Qt::CaseInsensitive) == 0)
							{
								if (item["hls"].string_value() != "0")
									streamInfo = "HLS";
								else
									streamInfo.clear();
							}
							else
							{
								const int bitrate = item["bitrate"].string_value().toInt();
								if (bitrate > 0)
									streamInfo += QString("\n%1 kbps").arg(bitrate);
							}
						}

						QString country = item["country"].string_value();
						if (!country.isEmpty())
						{
							const QString state = item["state"].string_value();
							if (!state.isEmpty() && country.compare(state, Qt::CaseInsensitive) != 0)
								country += "\n" + state;
						}

						const QList<QByteArray> tagsList = item["tags"].string_value().split(',');
						QString tags;
						for (const QByteArray &tagArr : tagsList)
						{
							QString tag = tagArr;
							if (!tag.isEmpty())
							{
								tag[0] = tag.at(0).toUpper();
								if (!tags.isEmpty())
									tags += ", ";
								tags += tag;
							}
						}

						const qint32 rating = item["votes"].string_value().toInt() - item["negativevotes"].string_value().toInt();

						m_rows.append(QSharedPointer<Column>(new Column {
							item["url"].string_value(),
							item["homepage"].string_value(),
							item["id"].string_value(),

							item["favicon"].string_value(),
							nullptr,
							radioIcon,

							item["name"].string_value(),
							streamInfo,
							country,
							tags,
							rating
						}));
					}
				}
				m_rowsToDisplay = m_rows;
				sort(m_sortColumnIdx, m_sortOrder);
				endInsertRows();
				emit radiosAdded();
			}
		}
		else
		{
			QPixmap *icon = nullptr;
			for (int r = 0; r < m_rows.size(); ++r)
			{
				Column *column = m_rows.at(r).data();
				if (column->iconReply == reply)
				{
					if (!icon)
					{
						const QImage image = QImage::fromData(reply->readAll());
						if (!image.isNull())
						{
							const int s = elementHeight();
							qreal devicePixelRatio = 1.0;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
							if (QWindow *win = Functions::getNativeWindow(m_widget))
								devicePixelRatio = win->devicePixelRatio();
#endif

							column->icon = QPixmap(s * devicePixelRatio, s * devicePixelRatio);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
							column->icon.setDevicePixelRatio(devicePixelRatio);
#endif
							column->icon.fill(Qt::transparent);

							QPainter painter(&column->icon);
							Functions::drawPixmap(painter, QPixmap::fromImage(image), m_widget, Qt::SmoothTransformation, Qt::KeepAspectRatio, {s, s});

							icon = &column->icon;
						}
					}
					else
					{
						column->icon = *icon;
					}
					emit dataChanged(QModelIndex(), QModelIndex());
				}
			}
		}
	}
	if (reply == m_replySearch)
		emit searchFinished();
	reply->deleteLater();
}