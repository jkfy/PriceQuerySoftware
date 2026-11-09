#include "mainwindow.h"
#include "importwizard.h"
#include "theme.h"
#include "data/dataservice.h"

#include <QApplication>
#include <QFont>
#include <QTimer>
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    QApplication::setOrganizationName("PriceQuery");
    QApplication::setApplicationName("PriceQuerySoftware");

    QFont font("Microsoft YaHei UI");
    font.setPointSize(9);
    app.setFont(font);
    app.setStyleSheet(theme::styleSheet());

    // 简易参数解析：方便命令行指定数据库，也方便自动截图
    QString host, user, pwd, dbName, shot;
    int  port  = 3306;
    int  page  = 0;
    int  wpage = 2;          // 向导默认截第 3 步（预检结果）
    bool demo  = false;
    bool wizard = false;

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString a = args.at(i);
        auto next = [&]() -> QString {
            return (i + 1 < args.size()) ? args.at(++i) : QString();
        };
        if (a == "--host")                  host = next();
        else if (a == "--port")             port = next().toInt();
        else if (a == "--user")             user = next();
        else if (a == "--password" || a == "--pwd") pwd = next();
        else if (a == "--dbname" || a == "--db")    dbName = next();
        else if (a == "--shot")             shot = next();
        else if (a == "--page")             page = next().toInt();
        else if (a == "--wpage")            wpage = next().toInt();
        else if (a == "--demo")             demo = true;
        else if (a == "--wizard")           wizard = true;
    }

    MainWindow window;

    if (!host.isEmpty() && !demo) {
        const QString db = dbName.isEmpty() ? "price_query" : dbName;
        window.connectOrDemo(host, port, db, user, pwd, false);
    }

    window.setPage(page);
    window.show();

    if (wizard) {
        auto *wiz = new ImportWizard(&window);
        wiz->setStartId(wpage);
        wiz->show();
        if (!shot.isEmpty()) {
            QTimer::singleShot(900, &app, [wiz, shot] {
                wiz->grab().save(shot);
                QApplication::quit();
            });
        }
    } else if (!shot.isEmpty()) {
        QTimer::singleShot(900, &app, [&window, shot] {
            window.grab().save(shot);
            QApplication::quit();
        });
    }

    return app.exec();
}
