#include <QLabel>
#include <QVBoxLayout>
#include "UserListView.h"
#include "GlobalVariables.h"

const QString UserProfileWidget::msEmphStyle =
"QLabel {"
"font-weight: bold;"
"font-size: 12px;"
"border: 1px;"
"color: #195BB2;"
"background-color: #E8F5FA;"
"border-radius: 4px;"
"}";
const QString UserProfileWidget::msUdstStyle =
"QLabel {"
"font-weight: bold;"
"font-size: 12px;"
"border: 1px;"
"color: rgb(224, 224, 224);"
"background-color: rgb(248, 248, 248);"
"border-radius: 4px;"
"}";

UserProfileWidget::UserProfileWidget()
{
	QVBoxLayout* layout = new QVBoxLayout();
	layout->setContentsMargins(gSpacing, gSpacing, gSpacing, gSpacing);
	{
		//user icon
		mIcon = new QLabel();
		mIcon->setAlignment(Qt::AlignCenter);
		layout->addWidget(mIcon);
		mRealName = new QLabel();
		mRealName->setTextInteractionFlags(Qt::TextSelectableByMouse);
		mRealName->setAlignment(Qt::AlignCenter);
		mRealName->setStyleSheet("font-weight: bold; font-size: 16px;");
		layout->addWidget(mRealName);
		mTitle = new QLabel();
		mTitle->setTextInteractionFlags(Qt::TextSelectableByMouse);
		mTitle->setAlignment(Qt::AlignCenter);
		mTitle->setStyleSheet("font-size: 16px;");
		layout->addWidget(mTitle);
		layout->addSpacing(gSpacing);

		mAttrLayout = new QHBoxLayout();
		mAttrLayout->setAlignment(Qt::AlignCenter);
		mAdmin = new QLabel("Admin", this);
		mAdmin->setStyleSheet(msUdstStyle);
		mAttrLayout->addWidget(mAdmin);
		mOwner = new QLabel("Owner", this);
		mOwner->setStyleSheet(msUdstStyle);
		mAttrLayout->addWidget(mOwner);
		mDeleted = new QLabel("Deleted", this);
		mDeleted->setStyleSheet(msUdstStyle);
		mAttrLayout->addWidget(mDeleted);
		layout->addLayout(mAttrLayout);

		const QString lstyle = "font-weight: bold; color: rgb(128, 128, 128);";
		const QString bstyle = "font-weight: bold;";

		QLabel* dn = new QLabel("Display name");
		dn->setStyleSheet(lstyle);
		layout->addWidget(dn);
		mDisplayName = new QLabel();
		mDisplayName->setStyleSheet(bstyle);
		mDisplayName->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(mDisplayName);
		layout->addSpacing(gSpacing);

		QLabel* tz = new QLabel("Time zone");
		tz->setStyleSheet(lstyle);
		layout->addWidget(tz);
		mTimeZone = new QLabel();
		mTimeZone->setStyleSheet(bstyle);
		mTimeZone->setTextInteractionFlags(Qt::TextSelectableByMouse);
		layout->addWidget(mTimeZone);
		layout->addSpacing(gSpacing);

		QLabel* ph = new QLabel("Phone number");
		ph->setStyleSheet(lstyle);
		layout->addWidget(ph);
		mPhone = new QLabel();
		mPhone->setOpenExternalLinks(true);
		mPhone->setStyleSheet("QMenu::item:disabled { color: grey; }"
							  "QMenu::item:selected { background-color: palette(Highlight); }");
		layout->addWidget(mPhone);
		layout->addSpacing(gSpacing);

		QLabel* em = new QLabel("Email address");
		em->setStyleSheet(lstyle);
		layout->addWidget(em);
		mEmail = new QLabel();
		mEmail->setOpenExternalLinks(true);
		mEmail->setStyleSheet("QMenu::item:disabled { color: grey; }"
							  "QMenu::item:selected { background-color: palette(Highlight); }");
		layout->addWidget(mEmail);
		layout->addSpacing(gSpacing);

		QLabel* sk = new QLabel("Skype");
		sk->setStyleSheet(lstyle);
		layout->addWidget(sk);
		mSkype = new QLabel();
		mSkype->setOpenExternalLinks(true);
		mSkype->setStyleSheet("QMenu::item:disabled { color: grey; }"
							  "QMenu::item:selected { background-color: palette(Highlight); }");
		layout->addWidget(mSkype);

		layout->addStretch();
	}
	this->setLayout(layout);
}
void UserProfileWidget::UpdateUserProfile(const User& user)
{
	QPixmap icon(user.GetPixmap());
	mIcon->setPixmap(icon);

	auto get_text = [](const QString& str) { return str.isEmpty() ? "Not set" : str; };

	mRealName->setText(get_text(user.GetRealName()));
	mTitle->setText(get_text(user.GetTitle()));
	mDisplayName->setText(get_text(user.GetName()));

	mTimeZone->setText(get_text(user.GetTimeZone()));
	mPhone->setText(user.GetPhone().isEmpty() ? "<b>Not set</b>" : ("<b><a href='tel:" + user.GetPhone() + "'>" + user.GetPhone() + "</a></b>"));
	mEmail->setText(user.GetEmail().isEmpty() ? "<b>Not set</b>" : ("<b><a href='mailto:" + user.GetEmail() + "'>" + user.GetEmail() + "</a></b>"));
	mSkype->setText(user.GetSkype().isEmpty() ? "<b>Not set</b>" : ("<b><a href='skype:" + user.GetSkype() + "?call'>" + user.GetSkype() + "</a></b>"));

	mAdmin->setStyleSheet(user.IsAdmin() ? msEmphStyle : msUdstStyle);
	mOwner->setStyleSheet(user.IsOwner() ? msEmphStyle : msUdstStyle);
	mDeleted->setStyleSheet(user.IsDeleted() ? msEmphStyle : msUdstStyle);
}
