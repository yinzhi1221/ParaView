/*=========================================================================

   Program: ParaView
   Module:    pqQuickLaunchDialog.h

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.1. 

   See License_v1.1.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#ifndef __pqQuickLaunchDialog_h 
#define __pqQuickLaunchDialog_h

#include <QDialog>
#include "pqComponentsExport.h"

/// A borderless pop-up dialog used to show actions that the user can launch.
/// Provides search capabilities.
class PQCOMPONENTS_EXPORT pqQuickLaunchDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;
public:
  pqQuickLaunchDialog(QWidget *parent=0);
  virtual ~pqQuickLaunchDialog();

  /// Set the actions to be launched using this dialog.
  void setActions(const QList<QAction*>& actions);
  void addActions(const QList<QAction*>& actions);

  virtual void accept();

protected slots:
  void currentRowChanged(int);
protected:
  virtual bool eventFilter(QObject *watched, QEvent *event);

  void updateSearch();
private:
  pqQuickLaunchDialog(const pqQuickLaunchDialog&); // Not implemented.
  void operator=(const pqQuickLaunchDialog&); // Not implemented.

  class pqInternal;
  pqInternal *Internal;
};

#endif


