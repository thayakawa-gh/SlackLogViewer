#ifndef CACHE_STATE_H
#define CACHE_STATE_H

#include <QFrame>
#include <QWidgetAction>
#include "GlobalVariables.h"

class Channel;
class QLabel;
class MessageListView;

class CacheStatus : public QFrame
{
	Q_OBJECT
public:

	//enum Type { TEXT, IMAGE, PDF, OTHERS, ALL, };
	enum Channel { ALLCHS, CURRENTCH, };

	CacheStatus(QWidget* parent);

	virtual void showEvent(QShowEvent* event);

Q_SIGNALS:

	//ch == -1は全チャンネル
	void CacheAllRequested(CacheStatus::Channel ch, CacheType type);
	void ClearCacheRequested(CacheType type);

private:

	QWidget* mParent;
	std::vector<QLabel*> mNum;
	std::vector<QLabel*> mSize;
	inline static QVector<CacheType> msTypeList
		= { CacheType::TEXT, CacheType::IMAGE, CacheType::PDF, CacheType::OTHERS, CacheType::ALL, };
};

struct CacheResult
{
	size_t num_downloaded;
	size_t num_exist;
	size_t num_failure;
};

CacheResult CacheAllFiles(Channel::Type ch_type, int ch_index, MessageListView* mes, CacheType type);
void ClearCache(CacheType type);

#endif