// Regression trigger: the numerical filter test passed while the real Qt
// entry had no usable Preview/result. Expected: menu -> dialog -> Preview ->
// Apply creates one result; changing shape/parameters and applying again
// updates the same model. Fix: "feat: validate ParaView-style region extraction UI".
// Find its SHA with git log --diff-filter=A --format="%h %s" -- Examples/Filter/Selection/TestExtractCellsByRegionUi.cpp
#include <IQComponents/igQtFilterDialogDockWidget.h>
#include <IQComponents/igQtModelDialogWidget.h>
#include <IQCore/igQtMainWindow.h>
#include <IQWidgets/igQtModelDrawWidget.h>
#include <iGameFileIO.h>
#include <iGameBoxStyle.h>
#include <iGameInteractor.h>
#include <iGameRadialStyle.h>
#include <iGameScene.h>
#include <iGameUnstructuredMesh.h>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QStringList>
#include <cmath>
#include <iostream>

static bool HasCount(igQtFilterDialogDockWidget* dialog, int cells) {
    const QString expected = QStringLiteral("预览：%1 个单元").arg(cells);
    for (auto* label : dialog->findChildren<QLabel*>())
        if (label->text() == expected) return true;
    return false;
}

static igm::vec2 Project(iGame::Interactor* interactor, const iGame::Point& point) {
    const auto clip = interactor->GetMVP() * igm::vec4(point[0], point[1], point[2], 1.f);
    return {(clip.x / clip.w + 1.f) * interactor->GetWidth() * 0.5f,
            (1.f - clip.y / clip.w) * interactor->GetHeight() * 0.5f};
}

static void Drag(iGame::Interactor* interactor, const igm::vec2& from,
                 const igm::vec2& to, iGame::MouseButton button) {
    interactor->FilterEvent({iGame::IEvent::MousePress, button, from, 0.});
    interactor->FilterEvent({iGame::IEvent::MouseMove, button, to, 0.});
    interactor->FilterEvent({iGame::IEvent::MouseRelease, button, to, 0.});
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    std::cerr << "Creating iGameVis window\n";
    igQtMainWindow window;
    window.show();
    app.processEvents();

    auto input = iGame::FileIO::ReadFile("./Models/ExtractCellsByRegion_BoxCases.vtk");
    if (!input) { std::cerr << "Could not load region fixture\n"; return 1; }
    input->SetName("ExtractCellsByRegion_BoxCases");
    window.modelTreeWidget->addDataObjectToModelTree(input, File);
    auto scene = window.rendererWidget->GetScene();
    if (!scene || !scene->GetCurrentModel()) { std::cerr << "Input is not selected\n"; return 2; }
    app.processEvents();

    auto* menu = window.findChild<QMenu*>(QStringLiteral("menu_developing_filters"));
    QAction* action = nullptr;
    if (menu) for (auto* candidate : menu->actions())
        if (candidate->data().toString() == QStringLiteral("extract_cells_by_region"))
            action = candidate;
    if (!action) { std::cerr << "Region menu entry missing\n"; return 3; }
    action->trigger();
    app.processEvents();
    igQtFilterDialogDockWidget* dialog = nullptr;
    for (auto* candidate : window.findChildren<igQtFilterDialogDockWidget*>())
        if (candidate->windowTitle() == QStringLiteral("按区域提取单元")) dialog = candidate;
    if (!dialog || !dialog->isVisible()) { std::cerr << "Region dialog missing\n"; return 4; }
    auto* previewButton = dialog->findChild<QPushButton*>(QStringLiteral("previewButton"));
    auto* applyButton = dialog->findChild<QPushButton*>(QStringLiteral("applyButton"));
    if (!previewButton || !previewButton->isVisible() || !applyButton || !applyButton->isVisible()) {
        std::cerr << "Region Preview button is hidden\n"; return 5;
    }

    const char* values[] = {"-1", "-1", "-1", "1", "1", "1"};
    for (int id = 1; id <= 6; ++id) {
        auto* edit = qobject_cast<QLineEdit*>(dialog->getWidget(id));
        if (!edit) return 6;
        edit->setText(QString::fromLatin1(values[id - 1]));
        QMetaObject::invokeMethod(edit, "editingFinished");
    }
    app.processEvents();
    previewButton->click();
    app.processEvents();
    if (!HasCount(dialog, 1)) { std::cerr << "Strict Box preview is not 1\n"; return 7; }
    if (argc > 1) dialog->grab().save(QString::fromLocal8Bit(argv[1]));

    auto* strict = qobject_cast<QCheckBox*>(dialog->getWidget(8));
    if (!strict) return 8;
    strict->setChecked(false);
    previewButton->click();
    app.processEvents();
    if (!HasCount(dialog, 3)) { std::cerr << "Loose Box preview is not 3\n"; return 9; }
    applyButton->click();
    app.processEvents();
    auto result = scene->GetCurrentModel();
    auto resultMesh = result ? iGame::DynamicCast<iGame::UnstructuredMesh>(result->GetDataObject()) : nullptr;
    if (!resultMesh || resultMesh->GetNumberOfCells() != 3) {
        std::cerr << "First Apply did not create visible 3-cell result\n"; return 10;
    }
    if (argc > 2) window.grab().save(QString::fromLocal8Bit(argv[2]));
    const auto resultId = scene->GetCurrentModelID();

    strict->setChecked(true);
    previewButton->click();
    app.processEvents();
    if (!HasCount(dialog, 1)) return 11;
    applyButton->click();
    app.processEvents();
    result = scene->GetCurrentModel();
    resultMesh = result ? iGame::DynamicCast<iGame::UnstructuredMesh>(result->GetDataObject()) : nullptr;
    if (scene->GetCurrentModelID() != resultId || !resultMesh || resultMesh->GetNumberOfCells() != 1) {
        std::cerr << "Second Apply failed to update the same result node\n"; return 12;
    }

    auto interactor = scene->GetInteractor();
    auto box = iGame::DynamicCast<iGame::BoxStyle>(interactor->GetSpecialInteractor("SelectBox"));
    if (!box || !box->GetBox()) { std::cerr << "Box widget missing\n"; return 17; }
    QStringList boxValuesBefore;
    for (int id = 1; id <= 6; ++id)
        boxValuesBefore << qobject_cast<QLineEdit*>(dialog->getWidget(id))->text();
    const auto boxHandle = Project(interactor, box->GetBox()->GetOpePoints()[0]);
    Drag(interactor, boxHandle, boxHandle + igm::vec2{60.f, 25.f}, iGame::LeftButton);
    app.processEvents();
    bool boxChanged = false;
    for (int id = 1; id <= 6; ++id)
        boxChanged |= qobject_cast<QLineEdit*>(dialog->getWidget(id))->text() != boxValuesBefore[id - 1];
    if (!boxChanged) {
        std::cerr << "Box drag did not synchronize coordinates\n"; return 18;
    }
    const auto boundsBeforeMiddle = box->GetBox()->GetExtremePoint();
    const auto edge = box->GetBox()->GetAllEdges()[0];
    const auto edgeHandle = Project(interactor, edge.first + (edge.second - edge.first) * 0.5f);
    Drag(interactor, edgeHandle, edgeHandle + igm::vec2{35.f, 20.f}, iGame::MiddleButton);
    app.processEvents();
    const auto boundsAfterMiddle = box->GetBox()->GetExtremePoint();
    if ((boundsAfterMiddle.first - boundsBeforeMiddle.first).length() < 1e-5 ||
        ((boundsAfterMiddle.second - boundsAfterMiddle.first) -
         (boundsBeforeMiddle.second - boundsBeforeMiddle.first)).length() > 1e-4) {
        std::cerr << "Box middle drag did not translate while preserving size\n"; return 25;
    }
    applyButton->click();
    app.processEvents();
    if (scene->GetCurrentModelID() != resultId) return 19;

    auto* shape = qobject_cast<QComboBox*>(dialog->getWidget(0));
    auto* radius = qobject_cast<QLineEdit*>(dialog->getWidget(7));
    if (!shape || !radius) return 13;
    shape->setCurrentIndex(1);
    for (int id = 1; id <= 3; ++id) {
        auto* edit = qobject_cast<QLineEdit*>(dialog->getWidget(id));
        edit->setText(QStringLiteral("0"));
        QMetaObject::invokeMethod(edit, "editingFinished");
    }
    radius->setText(QStringLiteral("1"));
    QMetaObject::invokeMethod(radius, "editingFinished");
    previewButton->click();
    app.processEvents();
    if (!HasCount(dialog, 1)) { std::cerr << "Sphere preview is not 1\n"; return 14; }
    auto sphere = iGame::DynamicCast<iGame::RadialStyle>(
        interactor->GetSpecialInteractor("ExtractCellsByRegionSphere"));
    if (!sphere) { std::cerr << "Sphere widget missing\n"; return 20; }
    const auto centerBefore = sphere->GetStartPoint();
    const double radiusBefore = (sphere->GetEndPoint() - centerBefore).length();
    const auto centerHandle = Project(interactor, centerBefore);
    Drag(interactor, centerHandle, centerHandle + igm::vec2{45.f, 25.f}, iGame::LeftButton);
    app.processEvents();
    if ((sphere->GetStartPoint() - centerBefore).length() < 1e-5 ||
        std::abs((sphere->GetEndPoint() - sphere->GetStartPoint()).length() - radiusBefore) > 1e-4) {
        std::cerr << "Sphere center drag failed or changed radius\n"; return 21;
    }
    const auto radiusHandle = Project(interactor, sphere->GetEndPoint());
    Drag(interactor, radiusHandle, radiusHandle + igm::vec2{55.f, 0.f}, iGame::LeftButton);
    app.processEvents();
    if (std::abs((sphere->GetEndPoint() - sphere->GetStartPoint()).length() - radiusBefore) < 1e-5) {
        std::cerr << "Sphere radius drag did not update radius\n"; return 22;
    }
    previewButton->click();
    app.processEvents();
    applyButton->click();
    app.processEvents();
    if (scene->GetCurrentModelID() != resultId) return 15;

    radius->setText(QStringLiteral("-1"));
    previewButton->click();
    app.processEvents();
    bool invalidShown = false;
    for (auto* label : dialog->findChildren<QLabel*>())
        invalidShown |= label->text() == QStringLiteral("区域参数无效");
    if (!invalidShown) return 16;
    radius->setText(QStringLiteral("1"));
    qobject_cast<QLineEdit*>(dialog->getWidget(1))->setText(QStringLiteral("100"));
    previewButton->click();
    app.processEvents();
    if (!HasCount(dialog, 0)) { std::cerr << "Empty preview is not 0\n"; return 23; }
    applyButton->click();
    app.processEvents();
    if (scene->GetCurrentModelID() != resultId) {
        std::cerr << "Empty Apply replaced the result\n"; return 24;
    }
    dialog->close();
    app.processEvents();
    std::cout << "iGameVis region menu, dialog, Box/Sphere Preview and reusable Apply passed\n";
    window.close();
    return 0;
}
