#pragma once

#include <QString>
#include <QList>
#include <QHash>

struct QuoteNode;

// ── 与界面解耦的传输对象 ────────────────────────────────────────────────
struct ItemRow
{
    qint64  id       = 0;
    QString code;
    QString name;
    QString type;      // 产品 / 部件 / 元件
    QString spec;
    QString unit;
    bool    hasPrice = false;
    double  price    = 0.0;
    QString priceDate;
};

struct BomRow
{
    qint64  id = 0;
    QString code;
    QString name;
    QString type;
    double  qty       = 1.0;
    double  unitPrice = 0.0;
    bool    hasPrice  = false;
};

struct PriceRow
{
    QString date;
    QString kind;     // 录入单价 / 历史成交
    double  amount = 0.0;
    QString source;
    QString user;
};

struct BatchRow
{
    QString time;
    QString type;
    QString file;
    int     total = 0;   // 0 表示 "—"（历史价格批次没有行数统计时）
    int     ok    = 0;
    int     fail  = 0;
    QString status;
};

// ── 数据服务 ────────────────────────────────────────────────────────────
// 设计要点：SQL 路径与演示路径共用同一套接口。
// 连不上数据库时自动降级为演示数据，保证 UI 草稿随时可以跑起来。
class DataService
{
public:
    static DataService &instance();

    bool    connectDb(const QString &host, int port, const QString &dbName,
                      const QString &user, const QString &pwd, QString *err);
    void    useDemo();
    bool    isConnected() const { return m_connected; }
    QString statusText() const;
    QString driverName() const { return m_driver; }

    QList<ItemRow>  items() const;
    QList<ItemRow>  components() const;          // 价格管理页：只看元件
    ItemRow         item(qint64 id) const;
    QList<BomRow>   bom(qint64 parentId) const;
    QList<PriceRow> prices(qint64 itemId) const;
    QList<BatchRow> batches() const;

    // 递归汇总：元件取单价，部件/产品取子件汇总。missing 累加缺价叶子数。
    double          rollupPrice(qint64 itemId, int *missing = nullptr) const;

    // 返回一棵待接管的树（调用方负责 delete）；失败返回 nullptr
    QuoteNode *productTree(QString *err) const;

private:
    DataService();
    void        loadDemoData();
    QuoteNode  *demoTree() const;
    void        recalc(QuoteNode *n) const;
    QString     odbcConnectionString(const QString &host, int port, const QString &db,
                                     const QString &user, const QString &pwd,
                                     const QString &driver) const;

    bool    m_connected = false;
    bool    m_demo      = true;
    QString m_driver;
    QString m_host = "127.0.0.1";
    int     m_port = 3306;
    QString m_db   = "price_query";

    // 演示数据
    QList<ItemRow>  m_demoItems;
    QList<PriceRow> m_demoPrices;              // 按 itemId 建索引用
    QHash<qint64, QList<PriceRow>> m_demoPriceMap;
    QHash<qint64, QList<QPair<qint64, double>>> m_demoEdges;  // parent -> [(child, qty)]
    QList<BatchRow> m_demoBatches;
    qint64          m_demoRootId = 1;
};
