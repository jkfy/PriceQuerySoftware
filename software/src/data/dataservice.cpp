#include "data/dataservice.h"
#include "model/quotanode.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QDate>
#include <QDebug>

namespace {

// 把字符串安全地嵌进 SQL 字面量。演示草稿用；正式版应全部改成 prepare + bindValue。
QString sqlStr(const QString &s)
{
    QString t = s;
    t.replace('\'', "''");
    return "'" + t + "'";
}

QStringList mysqlOdbcDrivers()
{
    return { "MySQL ODBC 9.5 Unicode Driver", "MySQL ODBC 9.0 Unicode Driver",
             "MySQL ODBC 8.5 Unicode Driver", "MySQL ODBC 8.0 Unicode Driver",
             "MySQL ODBC 9.5 ANSI Driver",    "MySQL ODBC 8.0 ANSI Driver" };
}

// 库里存的是枚举值，界面上要显示中文
QString cnType(const QString &t)
{
    if (t == "component") return QStringLiteral("元件");
    if (t == "assembly")  return QStringLiteral("部件");
    if (t == "product")   return QStringLiteral("产品");
    return t;
}

const char *kConn = "pq_main";

} // namespace

DataService &DataService::instance()
{
    static DataService s;
    return s;
}

DataService::DataService()
{
    loadDemoData();
}

QString DataService::odbcConnectionString(const QString &host, int port, const QString &db,
                                          const QString &user, const QString &pwd,
                                          const QString &driver) const
{
    return QString("DRIVER={%1};SERVER=%2;PORT=%3;DATABASE=%4;USER=%5;PASSWORD=%6;"
                   "CHARSET=utf8mb4;OPTION=3;")
        .arg(driver, host).arg(port).arg(db, user, pwd);
}

bool DataService::connectDb(const QString &host, int port, const QString &dbName,
                            const QString &user, const QString &pwd, QString *err)
{
    const QStringList drivers = QSqlDatabase::drivers();
    QStringList candidates;

    if (drivers.contains("QMYSQL"))
        candidates << "QMYSQL";
    if (drivers.contains("QODBC"))
        candidates << "QODBC";

    if (candidates.isEmpty()) {
        if (err) *err = "缺少数据库驱动：QMYSQL 与 QODBC 都不可用。";
        return false;
    }

    QString lastError;
    for (const QString &drv : candidates) {

        QSqlDatabase db = QSqlDatabase::contains(kConn)
                              ? QSqlDatabase::database(kConn)
                              : QSqlDatabase::addDatabase(drv, kConn);
        if (db.isOpen()) db.close();

        QStringList driverNames{ QString() };
        if (drv == "QODBC") driverNames = mysqlOdbcDrivers();

        for (const QString &odbcName : driverNames) {
            db = QSqlDatabase::database(kConn, false);
            if (db.isOpen()) db.close();

            if (drv == "QMYSQL") {
                db.setHostName(host);
                db.setPort(port);
                db.setUserName(user);
                db.setPassword(pwd);
                db.setDatabaseName(dbName);
            } else {
                db.setDatabaseName(odbcConnectionString(host, port, dbName, user, pwd, odbcName));
            }

            if (!db.open()) {
                lastError = QString("%1: %2").arg(drv, db.lastError().text().trimmed());
                continue;
            }

            QSqlQuery q(db);
            if (!q.exec("SELECT COUNT(*) FROM item")) {
                lastError = QString("已连接 %1，但找不到 price_query 的表结构，"
                                    "请先执行 db/01_schema.sql。").arg(dbName);
                db.close();
                continue;
            }

            m_connected = true;
            m_demo      = false;
            m_driver    = drv;
            m_host      = host;
            m_port      = port;
            m_db        = dbName;
            return true;
        }
    }

    if (err) *err = lastError;
    return false;
}

void DataService::useDemo()
{
    m_connected = false;
    m_demo      = true;
    m_driver.clear();
}

QString DataService::statusText() const
{
    if (m_connected)
        return QString("数据库已连接 · %1@%2:%3 (%4)").arg(m_db, m_host).arg(m_port).arg(m_driver);
    return "演示数据（未连接数据库）";
}

// ── 演示数据 ────────────────────────────────────────────────────────────
void DataService::loadDemoData()
{
    auto addItem = [this](qint64 id, const QString &code, const QString &name,
                          const QString &type, const QString &spec, const QString &unit,
                          bool hasPrice, double price, const QString &date) {
        ItemRow r;
        r.id = id; r.code = code; r.name = name; r.type = type;
        r.spec = spec; r.unit = unit;
        r.hasPrice = hasPrice; r.price = price; r.priceDate = date;
        m_demoItems << r;
    };

    addItem(1, "P-1001", "电气柜 A 型", "产品", "800×600×2000", "台", false, 0.0,   QString());
    addItem(2, "A-2001", "线束组件",   "部件", "主线束",        "套", false, 0.0,   QString());
    addItem(3, "A-2002", "主控模块",   "部件", "含控制器+显示", "套", false, 0.0,   QString());
    addItem(4, "C-3001", "接线端子",   "元件", "UK-2.5",        "个", true,  2.50,  "2026-01-01");
    addItem(5, "C-3002", "导线",       "元件", "RV 1.0",        "米", true,  1.20,  "2026-01-01");
    addItem(6, "C-3003", "控制器",     "元件", "PLC-24DI",      "个", true,  800.0, "2026-01-01");
    addItem(7, "C-3004", "显示屏",     "元件", "7寸触摸",       "个", true,  150.0, "2026-01-01");
    addItem(8, "C-3005", "保险丝",     "元件", "5×20 2A",       "个", false, 0.0,   QString());

    m_demoEdges.insert(1, { {2, 1.0}, {3, 1.0}, {8, 2.0} });
    m_demoEdges.insert(2, { {4, 10.0}, {5, 5.0} });
    m_demoEdges.insert(3, { {6, 1.0}, {7, 1.0} });

    auto addPrice = [this](qint64 itemId, const QString &date, const QString &kind,
                           double amount, const QString &source, const QString &user) {
        PriceRow p; p.date = date; p.kind = kind; p.amount = amount; p.source = source; p.user = user;
        m_demoPriceMap[itemId] << p;
    };

    addPrice(4, "2026-01-01", "录入单价", 2.50,  "手工",     "zhangwei");
    addPrice(5, "2026-01-01", "录入单价", 1.20,  "手工",     "zhangwei");
    addPrice(6, "2026-01-01", "录入单价", 800.0, "手工",     "zhangwei");
    addPrice(7, "2026-01-01", "录入单价", 150.0, "手工",     "zhangwei");
    addPrice(1, "2026-01-15", "历史成交", 981.0, "Excel 导入", "liuqing");
    addPrice(2, "2026-01-15", "历史成交", 31.0,  "Excel 导入", "liuqing");
    addPrice(3, "2026-01-15", "历史成交", 950.0, "Excel 导入", "liuqing");

    m_demoBatches = {
        { "2026-11-08 09:12", "报价查询", "客户BOM_1108.xlsx", 1284, 1277, 2, "部分成功" },
        { "2026-11-06 16:40", "历史价格", "2025年成交价汇总.xlsx", 356, 356, 0, "成功" },
        { "2026-11-01 10:03", "历史价格", "补录_2024.xlsx",        89, 87, 2, "失败" }
    };
}

void DataService::recalc(QuoteNode *n) const
{
    if (n->children.isEmpty()) {
        n->amount = n->qty * (n->hasPrice ? n->unitPrice : 0.0);
        n->missingInSubtree = !n->hasPrice;
        return;
    }
    double sum = 0.0;
    bool missing = false;
    for (QuoteNode *c : n->children) {
        recalc(c);
        sum += c->amount;
        missing = missing || c->missingInSubtree;
    }
    n->amount = n->qty * sum;
    n->missingInSubtree = missing;
}

QuoteNode *DataService::demoTree() const
{
    QHash<qint64, QuoteNode *> map;

    auto makeNode = [&](qint64 id) -> QuoteNode * {
        for (const ItemRow &r : m_demoItems) {
            if (r.id != id) continue;
            auto *n = new QuoteNode;
            n->code = r.code; n->name = r.name; n->type = r.type;
            n->hasPrice = r.hasPrice; n->unitPrice = r.price;
            map.insert(id, n);
            return n;
        }
        return nullptr;
    };

    QuoteNode *root = makeNode(m_demoRootId);
    if (!root) return nullptr;

    QList<qint64> frontier{ m_demoRootId };
    int depth = 0;
    while (!frontier.isEmpty() && depth < 20) {
        QList<qint64> next;
        for (qint64 pid : frontier) {
            QuoteNode *p = map.value(pid, nullptr);
            if (!p) continue;
            const auto edges = m_demoEdges.value(pid);
            for (const auto &e : edges) {
                QuoteNode *c = makeNode(e.first);
                if (!c) continue;
                c->qty = e.second;
                c->parent = p;
                p->children << c;
                if (c->type != "元件") next << e.first;
            }
        }
        frontier = next;
        ++depth;
    }

    recalc(root);
    return root;
}

// ── 查询入口 ────────────────────────────────────────────────────────────
QuoteNode *DataService::productTree(QString *err) const
{
    if (!m_connected) return demoTree();

    QSqlDatabase db = QSqlDatabase::database(kConn);

    qint64 rootId = 0;
    {
        QSqlQuery q(db);
        if (!q.exec("SELECT id FROM item WHERE item_type='product' AND status<>-1 "
                    "ORDER BY (item_code='P-1001') DESC, id LIMIT 1")) {
            if (err) *err = q.lastError().text();
            return nullptr;
        }
        if (!q.next()) {
            if (err) *err = "数据库里还没有任何产品，请先执行 db/03_sample_data.sql。";
            return nullptr;
        }
        rootId = q.value(0).toLongLong();
    }

    QHash<qint64, QuoteNode *> map;
    QuoteNode *root = nullptr;
    {
        QSqlQuery q(db);
        q.exec(QString("SELECT id,item_code,item_name,item_type FROM item WHERE id=%1").arg(rootId));
        if (q.next()) {
            root = new QuoteNode;
            root->code = q.value(1).toString();
            root->name = q.value(2).toString();
            root->type = "产品";
            map.insert(rootId, root);
        }
    }
    if (!root) return nullptr;

    QList<qint64> frontier{ rootId };
    int depth = 0;
    while (!frontier.isEmpty() && depth < 20) {
        QStringList ids;
        for (qint64 id : frontier) ids << QString::number(id);

        QSqlQuery q(db);
        const QString sql =
            QString("SELECT e.parent_id, c.id, c.item_code, c.item_name, c.item_type, e.quantity, p.amount "
                    "FROM bom_edge e JOIN item c ON c.id = e.child_id "
                    "LEFT JOIN v_item_direct_price p ON p.item_id = c.id "
                    "WHERE e.parent_id IN (%1) ORDER BY e.parent_id, e.seq_no").arg(ids.join(','));
        if (!q.exec(sql)) {
            if (err) *err = q.lastError().text();
            delete root;
            return nullptr;
        }

        QList<qint64> next;
        while (q.next()) {
            const qint64 pid = q.value(0).toLongLong();
            QuoteNode *p = map.value(pid, nullptr);
            if (!p) continue;
            auto *c = new QuoteNode;
            c->code = q.value(2).toString();
            c->name = q.value(3).toString();
            c->type = cnType(q.value(4).toString());
            c->qty = q.value(5).toDouble();
            const QVariant price = q.value(6);
            c->hasPrice = !price.isNull();
            c->unitPrice = price.toDouble();
            c->parent = p;
            p->children << c;
            map.insert(q.value(1).toLongLong(), c);
            if (c->type != "元件") next << q.value(1).toLongLong();
        }
        frontier = next;
        ++depth;
    }

    recalc(root);
    return root;
}

QList<ItemRow> DataService::items() const
{
    if (!m_connected) return m_demoItems;

    QList<ItemRow> out;
    QSqlQuery q(QSqlDatabase::database(kConn));
    if (!q.exec("SELECT i.id, i.item_code, i.item_name, i.item_type, i.spec, i.unit, "
                "p.amount, p.price_date "
                "FROM item i LEFT JOIN v_item_direct_price p ON p.item_id = i.id "
                "WHERE i.status <> -1 ORDER BY i.item_code"))
        return out;

    while (q.next()) {
        ItemRow r;
        r.id    = q.value(0).toLongLong();
        r.code  = q.value(1).toString();
        r.name  = q.value(2).toString();
        r.type  = cnType(q.value(3).toString());
        r.spec  = q.value(4).toString();
        r.unit  = q.value(5).toString();
        r.hasPrice  = !q.value(6).isNull();
        r.price     = q.value(6).toDouble();
        r.priceDate = q.value(7).toDate().toString("yyyy-MM-dd");
        out << r;
    }
    return out;
}

QList<ItemRow> DataService::components() const
{
    if (m_connected) {
        QList<ItemRow> all = items();
        QList<ItemRow> out;
        for (const ItemRow &r : all)
            if (r.type == "元件") out << r;
        return out;
    }
    QList<ItemRow> out;
    for (const ItemRow &r : m_demoItems)
        if (r.type == "元件") out << r;
    return out;
}

ItemRow DataService::item(qint64 id) const
{
    if (!m_connected) {
        for (const ItemRow &r : m_demoItems)
            if (r.id == id) return r;
        return ItemRow{};
    }
    // 草稿实现：直接复用 items()。正式版应改为按 id 的单独查询 + 缓存。
    for (const ItemRow &r : items())
        if (r.id == id) return r;
    return ItemRow{};
}

double DataService::rollupPrice(qint64 itemId, int *missing) const
{
    const ItemRow it = item(itemId);
    const QList<BomRow> kids = bom(itemId);

    if (kids.isEmpty()) {
        if (!it.hasPrice) {
            if (missing) ++(*missing);
            return 0.0;
        }
        return it.price;
    }

    double sum = 0.0;
    for (const BomRow &k : kids)
        sum += k.qty * rollupPrice(k.id, missing);
    return sum;
}

QList<BomRow> DataService::bom(qint64 parentId) const
{
    QList<BomRow> out;

    if (!m_connected) {
        const auto edges = m_demoEdges.value(parentId);
        for (const auto &e : edges) {
            for (const ItemRow &it : m_demoItems) {
                if (it.id != e.first) continue;
                BomRow b;
                b.id = it.id; b.code = it.code; b.name = it.name; b.type = it.type;
                b.qty = e.second; b.hasPrice = it.hasPrice; b.unitPrice = it.price;
                out << b;
                break;
            }
        }
        return out;
    }

    QSqlQuery q(QSqlDatabase::database(kConn));
    const QString sql =
        QString("SELECT c.id, c.item_code, c.item_name, c.item_type, e.quantity, p.amount "
                "FROM bom_edge e JOIN item c ON c.id = e.child_id "
                "LEFT JOIN v_item_direct_price p ON p.item_id = c.id "
                "WHERE e.parent_id = %1 ORDER BY e.seq_no").arg(parentId);
    if (!q.exec(sql)) return out;

    while (q.next()) {
        BomRow b;
        b.id   = q.value(0).toLongLong();
        b.code = q.value(1).toString();
        b.name = q.value(2).toString();
        b.type = cnType(q.value(3).toString());
        b.qty  = q.value(4).toDouble();
        b.hasPrice  = !q.value(5).isNull();
        b.unitPrice = q.value(5).toDouble();
        out << b;
    }
    return out;
}

QList<PriceRow> DataService::prices(qint64 itemId) const
{
    if (!m_connected) return m_demoPriceMap.value(itemId);

    QList<PriceRow> out;
    QSqlQuery q(QSqlDatabase::database(kConn));
    q.exec(QString("SELECT price_date, price_kind, amount, source, created_by "
                   "FROM price_record WHERE item_id = %1 ORDER BY price_date DESC").arg(itemId));
    while (q.next()) {
        PriceRow p;
        p.date   = q.value(0).toDate().toString("yyyy-MM-dd");
        p.kind   = q.value(1).toString() == "direct" ? "录入单价" : "历史成交";
        p.amount = q.value(2).toDouble();
        p.source = q.value(3).toString();
        p.user   = q.value(4).toString();
        out << p;
    }
    return out;
}

QList<BatchRow> DataService::batches() const
{
    if (!m_connected) return m_demoBatches;

    QList<BatchRow> out;
    QSqlQuery q(QSqlDatabase::database(kConn));
    q.exec("SELECT created_at, batch_type, file_name, total_rows, ok_rows, fail_rows, status "
           "FROM import_batch ORDER BY created_at DESC");
    while (q.next()) {
        BatchRow b;
        b.time  = q.value(0).toDateTime().toString("yyyy-MM-dd HH:mm");
        b.type  = q.value(1).toString() == "history_price" ? "历史价格" : "报价查询";
        b.file  = q.value(2).toString();
        b.total = q.value(3).toInt();
        b.ok    = q.value(4).toInt();
        b.fail  = q.value(5).toInt();
        b.status = q.value(6).toString();
        out << b;
    }
    return out;
}
