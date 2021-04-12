#ifndef USER_LIST_VIEW_H
#define USER_LIST_VIEW_H

#include <QListView>

class QLabel;
class User;

class UserProfileWidget : public QFrame
{
public:

	UserProfileWidget();

public slots:

	void UpdateUserProfile(const User& user);

private:

	QLabel* mIcon;
	QLabel* mRealName;
	QLabel* mTitle;
	QLabel* mDisplayName;
	QLabel* mTimeZone;
	QLabel* mPhone;
	QLabel* mEmail;
	QLabel* mSkype;
	QLabel* mAdmin;
	QLabel* mOwner;
	QLabel* mDeleted;

	QHBoxLayout* mAttrLayout;

	static const QString msEmphStyle;
	static const QString msUdstStyle;
};

class User;

class UserListView : public QListView
{
public:


private:

	QList<User*> mUsers;
	QList<User*> mDeletedUsers;
};

#endif