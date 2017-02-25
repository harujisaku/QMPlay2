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

#include <MediaBrowser/ProstoPleer.hpp>

#include <QMPlay2Extensions.hpp>
#include <NetworkAccess.hpp>
#include <Functions.hpp>
#include <Json11.hpp>

#include <QTextDocument>
#include <QHeaderView>
#include <QTreeWidget>
#include <QAction>

constexpr char g_name[] = "Prostopleer";
constexpr char g_url[]  = "http://pleer.net";

/**/

void ProstoPleer::prepareWidget(QTreeWidget *treeW)
{
	MediaBrowserCommon::prepareWidget(treeW);

	treeW->headerItem()->setHidden(false);
	treeW->setIconSize(QSize(22, 22));

	treeW->headerItem()->setText(0, tr("Title"));
	treeW->headerItem()->setText(1, tr("Artist"));
	treeW->headerItem()->setText(2, tr("Length"));
	treeW->headerItem()->setText(3, tr("Bitrate"));

	Functions::setheaderSectionResizeMode(treeW->header(), 2, QHeaderView::ResizeToContents);
	Functions::setheaderSectionResizeMode(treeW->header(), 3, QHeaderView::ResizeToContents);
}

QString ProstoPleer::name() const
{
	return g_name;
}
QIcon ProstoPleer::icon() const
{
	return QIcon(":/prostopleer");
}

QString ProstoPleer::getQMPlay2Url(const QString &text)
{
	return QString("%1://{%2}").arg(g_name, getWebpageUrl(text));
}

NetworkReply *ProstoPleer::getSearchReply(const QString &text, const qint32 page, NetworkAccess &net)
{
	return net.start(QString("%1/search?q=%2&page=%3").arg(g_url, text).arg(page));
}
void ProstoPleer::addSearchResults(const QByteArray &reply, QTreeWidget *treeW)
{
	const QIcon prostopleerIcon(":/prostopleer");

	QRegExp regexp("<li duration=\"([\\d]+)\"\\s+file_id=\"([^\"]+)\"\\s+singer=\"([^\"]+)\"\\s+song=\"([^\"]+)\"\\s+link=\"([^\"]+)\"\\s+rate=\"([^\"]+)\"\\s+size=\"([^\"]+)\"");
	regexp.setMinimal(true);

	QTextDocument txtDoc;

	int offset = 0;
	while ((offset = regexp.indexIn(reply, offset)) != -1)
	{
		QTreeWidgetItem *tWI = new QTreeWidgetItem(treeW);
		tWI->setData(0, Qt::UserRole, regexp.cap(5)); //file_id
		tWI->setIcon(0, prostopleerIcon);

		txtDoc.setHtml(regexp.cap(4));
		tWI->setText(0, txtDoc.toPlainText());
		tWI->setToolTip(0, txtDoc.toPlainText());

		txtDoc.setHtml(regexp.cap(3));
		tWI->setText(1, txtDoc.toPlainText());
		tWI->setToolTip(1, txtDoc.toPlainText());

		int time = regexp.cap(1).toInt();
		tWI->setText(2, Functions::timeToStr(time));

		QString bitrate = regexp.cap(6).toLower().remove(' ').replace('/', 'p');
		if (bitrate == "vbr" && time > 0)
		{
			const QStringList fSizeList = regexp.cap(7).toLower().split(' ');
			if (fSizeList.count() >= 2 && fSizeList[1] == "mb")
			{
				float fSize = fSizeList[0].toFloat();
				if (fSize > 0.0f)
				{
					fSize *= 8.0f * 1024.0f;
					bitrate = QString("%1kbps").arg((int)(fSize / time), 3);
				}
			}
		}
		else if (bitrate.length() == 6)
			bitrate.prepend(" ");
		else if (bitrate.length() == 5)
			bitrate.prepend("  ");
		tWI->setText(3, bitrate);

		offset += regexp.matchedLength();
	}
}

bool ProstoPleer::hasWebpage()
{
	return true;
}
QString ProstoPleer::getWebpageUrl(const QString &text)
{
	return QString("%1/en/tracks/%2").arg(g_url, text);
}

bool ProstoPleer::hasCompleter()
{
	return true;
}
NetworkReply *ProstoPleer::getCompleterReply(const QString &text, NetworkAccess &net)
{
	return net.start(QString("%1/search_suggest").arg(g_url), QByteArray("part=" + text.toUtf8()), NetworkAccess::UrlEncoded);
}
QStringList ProstoPleer::getCompletions(const QByteArray &reply)
{
	const int idx1 = reply.indexOf("[");
	const int idx2 = reply.lastIndexOf("]");
	if (idx1 > -1 && idx2 > idx1)
	{
		QTextDocument txtDoc;
		txtDoc.setHtml(reply.mid(idx1 + 1, idx2 - idx1 - 1));
		return txtDoc.toPlainText().remove('"').split(',');
	}
	return {};
}

QMPlay2Extensions::AddressPrefix ProstoPleer::addressPrefixList(bool img) const
{
	return QMPlay2Extensions::AddressPrefix(g_name, img ? QImage(":/prostopleer") : QImage());
}

QAction *ProstoPleer::getAction() const
{
	QAction *act = new QAction(tr("Search on Prostopleer"), nullptr);
	act->setIcon(QIcon(":/prostopleer"));
	return act;
}

bool ProstoPleer::convertAddress(const QString &prefix, const QString &url, QString *streamUrl, QString *name, QImage *img, QString *extension, IOController<> *ioCtrl)
{
	Q_UNUSED(name)
	if (prefix == g_name)
	{
		if (streamUrl || img)
		{
			if (img)
				*img = QImage(":/prostopleer");
			if (extension)
				*extension = ".mp3";
			if (ioCtrl && streamUrl)
			{
				QString fileId = url;
				while (fileId.endsWith('/'))
					fileId.truncate(1);

				const int idx = url.lastIndexOf('/');
				if (idx > -1)
				{
					NetworkAccess net;
					net.setMaxDownloadSize(0x200000 /* 2 MiB */);

					IOController<NetworkReply> &netReply = ioCtrl->toRef<NetworkReply>();
					net.start(netReply, QString("%1/site_api/files/get_url?id=%2").arg(g_url, fileId.mid(idx + 1)));
					netReply->waitForFinished();
					if (!netReply->hasError())
					{
						const Json json = Json::parse(netReply->readAll());
						const QString tmpStreamUrl = json["track_link"].string_value();
						if (!tmpStreamUrl.isEmpty())
							*streamUrl = tmpStreamUrl;
					}
					else if (netReply->error() != NetworkReply::Error::Aborted)
						QMPlay2Core.sendMessage(tr("Try again later"), g_name);
					netReply.clear();
				}
			}
		}
		return true;
	}
	return false;
}
