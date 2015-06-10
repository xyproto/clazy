/**********************************************************************
**  Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
**  Author: Sérgio Martins <sergio.martins@kdab.com>
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2.1 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL.txt included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**********************************************************************/

#include "requiredresults.h"
#include "Utils.h"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>

using namespace clang;

RequiredResults::RequiredResults(clang::CompilerInstance &ci)
    : CheckBase(ci)
{
}

bool RequiredResults::shouldIgnoreMethod(const std::string &qualifiedName)
{
    static std::vector<std::string> files;
    if (files.empty()) {
        files.reserve(31);
        files.push_back("QDir::mkdir");
        files.push_back("QDir::rmdir");
        files.push_back("QDir::mkpath");
        files.push_back("QDBusConnection::send");

        files.push_back("QRegExp::indexIn");
        files.push_back("QRegExp::exactMatch");
        files.push_back("QQmlProperty::write");
        files.push_back("QQmlProperty::reset");
        files.push_back("QWidget::winId");
        files.push_back("QtWaylandClient::QWaylandEglWindow::contentFBO");
        files.push_back("ProString::updatedHash");

        // kdepim
        files.push_back("KCalCore::Incidence::recurrence");
        files.push_back("KCalCore::RecurrenceRule::Private::buildCache");
        files.push_back("KAlarmCal::KAEvent::updateKCalEvent");
        files.push_back("Akonadi::Server::Collection::clearMimeTypes");
        files.push_back("Akonadi::Server::Collection::addMimeType");
        files.push_back("Akonadi::Server::PimItem::addFlag");
        files.push_back("Akonadi::Server::PimItem::addTag");

        // kf5 libs
        files.push_back("KateVi::Command::execute");
        files.push_back("KArchiveDirectory::copyTo");
        files.push_back("KBookmarkManager::saveAs");
        files.push_back("KBookmarkManager::save");
        files.push_back("KLineEditPrivate::copySqueezedText");
        files.push_back("KJS::UString::Rep::hash");
        files.push_back("KCModuleProxy::realModule");
        files.push_back("KCategorizedView::visualRect");
        files.push_back("KateLineLayout::textLine");
        files.push_back("DOM::HTMLCollectionImpl::firstItem");
        files.push_back("DOM::HTMLCollectionImpl::nextItem");
        files.push_back("DOM::HTMLCollectionImpl::firstItem");
        files.push_back("ImapResourceBase::settings");
    }

    return std::find(files.cbegin(), files.cend(), qualifiedName) != files.cend();
}

void RequiredResults::VisitStmt(clang::Stmt *stm)
{
    auto compound = dyn_cast<CompoundStmt>(stm);
    if (compound == nullptr)
        return;

    for (auto it = compound->child_begin(), end = compound->child_end(); it != end ; ++it) {
        auto callExpr = dyn_cast<CXXMemberCallExpr>(*it);
        if (callExpr == nullptr)
            continue;

        CXXMethodDecl *methodDecl =	callExpr->getMethodDecl();
        if (methodDecl == nullptr || !methodDecl->isConst())
            continue;

        std::string methodName = methodDecl->getQualifiedNameAsString();
        if (shouldIgnoreMethod(methodName)) // Filter out some false positives
            continue;

        QualType qt = methodDecl->getReturnType();
        const Type *type = qt.getTypePtrOrNull();
        if (type == nullptr || type->isVoidType())
            continue;

        // Bail-out if any parameter is a non-const-ref or pointer
        auto it2 = methodDecl->param_begin();
        auto e = methodDecl->param_end();
        bool bailout = false;
        for (; it2 != e ; ++it2) {
            auto paramVarDecl = *it2;
            QualType qt = paramVarDecl->getType();
            const Type *type = qt.getTypePtrOrNull();
            if (type == nullptr || type->isPointerType()) {
                bailout = true;
                break;
            }

            // qt.isConstQualified() not working !? TODO: Replace this string parsing when I figure it out
            if (type->isReferenceType() && qt.getAsString().find("const ") == std::string::npos) {
                bailout = true;
                break;
            }
        }

        if (!bailout) {
            std::string error = std::string("Unused result of const member (") + methodName + std::string(") [-Wmore-warnings-unused-result]");
            Utils::emitWarning(m_ci, callExpr->getLocStart(), error.c_str());
        }
    }
}

std::string RequiredResults::name() const
{
    return "unused-result";
}