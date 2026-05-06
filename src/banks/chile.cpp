#include "banks/chile.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QTime>

namespace pdfparser {

namespace {

// Chile-specific: rows always start with "DD/MM ".
const QRegularExpression &rowStartRx()
{
    static const QRegularExpression rx(
        QStringLiteral(R"(^\s*\d{1,2}/\d{1,2}\b)"));
    return rx;
}

// Chile column layout: <DD/MM> <description...> <n1> [<n2>]
//   When n2 is present they are the two columns (CARGOS / ABONOS) — the
//   non-zero one is the actual movement.
//   When only n1 is present, it's the single posted amount.
const QRegularExpression &rowRx()
{
    static const QRegularExpression rx(
        QStringLiteral(
            R"(^(?<date>\d{1,2}/\d{1,2})\s+)"
            R"((?<desc>.+?)\s+)"
            R"((?<n1>-?[\d\.,]+))"
            R"((?:\s+(?<n2>-?[\d\.,]+))?\s*$)"
        ));
    return rx;
}

// Year doesn't appear on each Chile row (only DD/MM). Pull it from the
// statement header line:
//     "TELEFONO : 562547069 DESDE : 27/02/2026 HASTA : 31/03/2026"
// Use the DESDE (start) year so a row like "31/12" lands in the right year
// even when the period crosses Dec→Jan. Fall back to a bare 20XX scan, then
// to the current year, if the header isn't found.
int statementYear(const QStringList &pages)
{
    static const QRegularExpression desdeRx(
        QStringLiteral(R"(DESDE\s*:?\s*\d{1,2}/\d{1,2}/(?<from>\d{4}))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression yearRx(QStringLiteral(R"(\b(20\d{2})\b)"));

    for (const QString &p : pages) {
        const auto m = desdeRx.match(p);
        if (m.hasMatch()) return m.captured("from").toInt();
    }
    if (!pages.isEmpty()) {
        const auto m = yearRx.match(pages.first());
        if (m.hasMatch()) return m.captured(1).toInt();
    }
    return QDate::currentDate().year();
}

} // namespace

Chile::Chile(const QString &typeAccount)
    : Bank(QStringLiteral("Banco de Chile"), typeAccount) {}

Chile::Chile(const QString &typeAccount, const QString &filePath)
    : Bank(QStringLiteral("Banco de Chile"), typeAccount, filePath) {}

void Chile::readBankMovementsDebit(const QStringList &pagesText,
                                   QList<Transaction> &out)
{
    const int year = statementYear(pagesText);

    auto flush = [&](const QString &row) {
        if (row.isEmpty()) return;
        const auto m = rowRx().match(row);
        if (!m.hasMatch()) return;

        const QStringList dm = m.captured("date").split(QChar('/'));
        if (dm.size() != 2) return;
        const int day = dm[0].toInt();
        const int mon = dm[1].toInt();
        if (mon < 1 || mon > 12 || day < 1 || day > 31) return;

        const QString rawDesc = m.captured("desc").simplified();

        const double n1 = parseClpAmount(m.captured("n1"));
        const QString n2s = m.captured("n2");
        double amount = n1;
        if (!n2s.isEmpty()) {
            const double n2 = parseClpAmount(n2s);
            // Two columns shown: cargos | abonos. Whichever column is
            // non-zero carries the actual movement.
            amount = (n2 > 0.0) ? n2 : n1;
        }

        Transaction t;
        t.date        = QDateTime(QDate(year, mon, day), QTime(0, 0));
        t.amount      = qAbs(amount);
        t.account     = nameBank;
        t.category    = classifier.classify(rawDesc);
        t.description = cleanDescription(rawDesc);
        out.append(t);
    };

    QString current;
    bool inTable    = false;
    bool reachedEnd = false;

    for (int p = 0; p < pagesText.size() && !reachedEnd; ++p) {
        const QStringList lines = pagesText.at(p).split(QChar('\n'),
                                                        Qt::KeepEmptyParts);
        for (int l = 0; l < lines.size(); ++l) {
            const QString line = lines.at(l).trimmed();

            if (!inTable) {
                if (line.contains(QStringLiteral("DIA/MES"),
                                  Qt::CaseInsensitive))
                    inTable = true;
                continue;
            }

            if (line.contains(QStringLiteral("SALDO FINAL"),
                              Qt::CaseInsensitive)) {
                reachedEnd = true;
                break;
            }
            if (line.contains(QStringLiteral("SALDO INICIAL"),
                              Qt::CaseInsensitive))
                continue;
            if (line.isEmpty()) continue;

            if (rowStartRx().match(line).hasMatch()) {
                flush(current);
                current = line;
            } else if (!current.isEmpty()) {
                current.append(QChar(' ')).append(line);
            }
        }
    }
    flush(current);
}

void Chile::readBankMovementsCredit(const QStringList &pagesText,
                                    QList<Transaction> &out)
{
    // Section start: anything before "PERÍODO ACTUAL" / "PERIODO ACTUAL"
    // is the statement summary and must be skipped.
    static const QRegularExpression sectionStartRx(
        QStringLiteral(R"(PER[ÍI]ODO\s+ACTUAL)"),
        QRegularExpression::CaseInsensitiveOption);

    // Row layout (one per credit-card purchase):
    //   <CITY> <DD/MM/YY> <auth> <merchant...> $ <orig> $ <clp> <cur>/<tot> $ <cuotaAmt>
    // The merchant is the only variable-width middle column, so we anchor on
    // everything around it.
    static const QRegularExpression rowRx(
        QStringLiteral(
            R"(^(?<city>[A-Z][A-Z\s\.&\-]+?)\s+)"
            R"((?<date>\d{2}/\d{2}/\d{2})\s+)"
            R"((?<auth>\d{6,})\s+)"
            R"((?<merchant>.+?)\s+)"
            R"(\$\s*(?<orig>[\d\.,]+)\s+)"
            R"(\$\s*(?<clp>[\d\.,]+)\s+)"
            R"((?<cur>\d{1,2})/(?<tot>\d{1,2})\s+)"
            R"(\$\s*(?<cuotaAmt>[\d\.,]+)\s*$)"
        ));

    // A new row always starts with "<CITY> DD/MM/YY"; anything else that
    // shows up between rows is a wrap of the previous one.
    static const QRegularExpression rowStartRx(
        QStringLiteral(R"(^[A-Z][A-Z\s\.&\-]+?\s+\d{2}/\d{2}/\d{2}\s+\d{6,}\b)"));

    auto flush = [&](const QString &row) {
        if (row.isEmpty()) return;
        const auto m = rowRx.match(row);
        if (!m.hasMatch()) return;

        const QStringList ymd = m.captured("date").split(QChar('/'));
        if (ymd.size() != 3) return;
        const int dd = ymd[0].toInt();
        const int mm = ymd[1].toInt();
        const int yy = ymd[2].toInt();
        const QDate baseDate(2000 + yy, mm, dd);
        if (!baseDate.isValid()) return;

        const QString merchant = m.captured("merchant").simplified();
        const int     totCuotas = qMax(1, m.captured("tot").toInt());
        const double  cuotaAmt  = parseClpAmount(m.captured("cuotaAmt"));
        const QString rawDesc   = merchant;

        // One Transaction per scheduled installment, dated one month apart
        // starting from the purchase date. (cuota 01/01 -> a single tx.)
        for (int i = 0; i < totCuotas; ++i) {
            Transaction t;
            t.date    = QDateTime(baseDate.addMonths(i), QTime(0, 0));
            t.amount  = qAbs(cuotaAmt);
            t.account = nameBank;
            t.category = classifier.classify(rawDesc);
            t.description = QStringLiteral("%1 (cuota %2/%3)")
                                .arg(cleanDescription(rawDesc))
                                .arg(i + 1)
                                .arg(totCuotas);
            out.append(t);
        }
    };

    QString current;
    bool inSection = false;

    for (const QString &page : pagesText) {
        const QStringList lines = page.split(QChar('\n'), Qt::KeepEmptyParts);
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();

            if (!inSection) {
                if (sectionStartRx.match(line).hasMatch())
                    inSection = true;
                continue;
            }
            if (line.isEmpty()) continue;

            if (rowStartRx.match(line).hasMatch()) {
                flush(current);
                current = line;
            } else if (!current.isEmpty()) {
                current.append(QChar(' ')).append(line);
            }
        }
    }
    flush(current);
}

} // namespace pdfparser
