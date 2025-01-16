#pragma once

#include <QGroupBox>
#include <QMap>
#include <QMargins>
#include <QPair>
#include <QToolButton>

/* 
 * Code taken with thanks from answer provided by stackoverflow user1134621
 * https://stackoverflow.com/questions/37049588/making-collapsible-groupboxes-in-qt-what-determines-the-collapsed-size
 * copyright https://creativecommons.org/licenses/by-sa/3.0/ 
 * 
 * Some changes made to fix behaviour. Removed custom expand/collapse button.
*/

class QResizeEvent;
class QSpacerItem;

class CollapsibleGroupBox : public QGroupBox
{
  public:
  explicit CollapsibleGroupBox(QString const& title, QWidget* parent = nullptr);

  protected:
  void resizeEvent(QResizeEvent*) override;

  private:
  void resizeCollapseButton();
  void collapseLayout(QLayout* layout);
  void collapseSpacer(QSpacerItem* spacer);
  void expandLayout(QLayout* layout);
  void expandSpacer(QSpacerItem* spacer);

  QToolButton* m_clExpButton;
  QMap<void const*, QMargins> m_layoutMargins;
  QMap<void const*, QPair<QSize, QSizePolicy>> m_spacerSizes;

  private slots:
  void onScreenChanged();
  void onVisibilityChanged(bool checked);
};
