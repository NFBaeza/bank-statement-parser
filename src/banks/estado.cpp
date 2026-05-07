#include "banks/estado.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QTime>

namespace pdfparser {

namespace {

// ============================================================================
// Standard cartola — header "CARTOLA CUENTARUT N° ...".
//   Row example:  15/Dic 8021402 TEF A SOMEBODY $20.000 $111.947
//   Layout:       <DD>/<Mon> <op> <description...> $<amount> $<balance>
// ============================================================================

const QRegularExpression &standardRowStartRx()
{
    static const QRegularExpression rx(
        QStringLiteral(R"(^\s*\d{1,2}/[A-Za-zñÑ]{3,}\.?\s+\d+\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx;
}

const QRegularExpression &standardRowRx()
{
    static const QRegularExpression rx(
        QStringLiteral(
            R"(^(?<day>\d{1,2})/(?<mon>[A-Za-zñÑ]{3,})\.?\s+)"
            R"((?<op>\d+)\s+)"
            R"((?<desc>.+?)\s+)"
            R"(\$\s*(?<amount>-?[\d\.,]+)\s+)"
            R"(\$\s*(?<balance>-?[\d\.,]+))"
        ),
        QRegularExpression::CaseInsensitiveOption);
    return rx;
}

int standardStatementYear(const QStringList &pages)
{
    static const QRegularExpression headerRx(
        QStringLiteral(R"(Cartola\s+\w+\s+\d+\s+\d{1,2}/\d{1,2}/(?<year>\d{4}))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bareYearRx(QStringLiteral(R"(\b(20\d{2})\b)"));

    for (const QString &p : pages) {
        const auto m = headerRx.match(p);
        if (m.hasMatch()) return m.captured("year").toInt();
    }
    if (!pages.isEmpty()) {
        const auto m = bareYearRx.match(pages.first());
        if (m.hasMatch()) return m.captured(1).toInt();
    }
    return QDate::currentDate().year();
}

// ============================================================================
// Provisional cartola — header "Últimos Movimientos CuentaRUT".
//   Row example:  05/05/2026 8041880 PAGO TOTTUS RANCAGUA -16.007 0 60.100
//   Layout:       <DD/MM/YYYY> <op> <description...> <cargo> <abono> <balance>
// Cargo column is signed (negative = outflow); abono column is positive
// when there's an incoming amount, otherwise 0.
// ============================================================================

const QRegularExpression &provisionalRowStartRx()
{
    static const QRegularExpression rx(
        QStringLiteral(R"(^\s*\d{1,2}/\d{1,2}/\d{4}\s+\d+\b)"));
    return rx;
}

const QRegularExpression &provisionalRowRx()
{
    static const QRegularExpression rx(
        QStringLiteral(
            R"(^(?<day>\d{1,2})/(?<mon>\d{1,2})/(?<year>\d{4})\s+)"
            R"((?<op>\d+)\s+)"
            R"((?<desc>.+?)\s+)"
            R"((?<cargo>-?[\d\.,]+)\s+)"
            R"((?<abono>-?[\d\.,]+)\s+)"
            R"((?<balance>-?[\d\.,]+))"
        ));
    return rx;
}

// ============================================================================

bool isProvisionalCartola(const QStringList &pages)
{
    if (pages.isEmpty()) return false;
    return pages.first().contains(
        QStringLiteral("Últimos Movimientos"), Qt::CaseInsensitive);
}

void parseStandardCartola(const QStringList &pagesText,
                          const QString &nameBank,
                          SimpleClassifier &classifier,
                          QList<Bank::Transaction> &out)
{
    const int year = standardStatementYear(pagesText);

    auto flush = [&](const QString &row) {
        if (row.isEmpty()) return;
        const auto m = standardRowRx().match(row);
        if (!m.hasMatch()) return;

        const int day = m.captured("day").toInt();
        const int mon = Bank::spanishMonth(m.captured("mon"));
        if (mon == 0 || day < 1 || day > 31) return;

        // Description = between op and amount, plus wrap text after balance.
        QString rawDesc = m.captured("desc").simplified();
        const int balanceEnd = m.capturedEnd("balance");
        if (balanceEnd >= 0 && balanceEnd < row.size()) {
            const QString tail = row.mid(balanceEnd).trimmed();
            if (!tail.isEmpty())
                rawDesc.append(QChar(' ')).append(tail);
        }

        Bank::Transaction t;
        t.date        = QDateTime(QDate(year, mon, day), QTime(0, 0));
        t.amount      = qAbs(Bank::parseClpAmount(m.captured("amount")));
        t.account     = nameBank;
        t.category    = classifier.classify(rawDesc);
        t.description = Bank::cleanDescription(rawDesc);
        out.append(t);
    };

    QString current;
    bool reachedEnd = false;
    for (int p = 0; p < pagesText.size() && !reachedEnd; ++p) {
        const QStringList lines = pagesText.at(p).split(QChar('\n'),
                                                        Qt::KeepEmptyParts);
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            if (line.contains(QStringLiteral("subtotal"),
                              Qt::CaseInsensitive)) {
                reachedEnd = true;
                break;
            }

            if (standardRowStartRx().match(line).hasMatch()) {
                flush(current);
                current = line;
            } else if (!current.isEmpty()) {
                current.append(QChar(' ')).append(line);
            }
        }
    }
    flush(current);
}

void parseProvisionalCartola(const QStringList &pagesText,
                             const QString &nameBank,
                             SimpleClassifier &classifier,
                             QList<Bank::Transaction> &out)
{
    auto flush = [&](const QString &row) {
        if (row.isEmpty()) return;
        const auto m = provisionalRowRx().match(row);
        if (!m.hasMatch()) return;

        const int day  = m.captured("day").toInt();
        const int mon  = m.captured("mon").toInt();
        const int year = m.captured("year").toInt();
        if (mon < 1 || mon > 12 || day < 1 || day > 31) return;

        QString rawDesc = m.captured("desc").simplified();
        const int balanceEnd = m.capturedEnd("balance");
        if (balanceEnd >= 0 && balanceEnd < row.size()) {
            const QString tail = row.mid(balanceEnd).trimmed();
            if (!tail.isEmpty())
                rawDesc.append(QChar(' ')).append(tail);
        }

        // Movement is whichever of (cargo|abono) is non-zero. Cargo is
        // typically printed with a leading "-"; we store an unsigned amount
        // (direction stays in the description / category).
        const double cargo = Bank::parseClpAmount(m.captured("cargo"));
        const double abono = Bank::parseClpAmount(m.captured("abono"));
        const double amount = (qAbs(abono) > 0.0) ? abono : cargo;

        Bank::Transaction t;
        t.date        = QDateTime(QDate(year, mon, day), QTime(0, 0));
        t.amount      = qAbs(amount);
        t.account     = nameBank;
        t.category    = classifier.classify(rawDesc);
        t.description = Bank::cleanDescription(rawDesc);
        out.append(t);
    };

    QString current;
    bool reachedEnd = false;
    for (int p = 0; p < pagesText.size() && !reachedEnd; ++p) {
        const QStringList lines = pagesText.at(p).split(QChar('\n'),
                                                        Qt::KeepEmptyParts);
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            if (line.contains(QStringLiteral("subtotal"),
                              Qt::CaseInsensitive)) {
                reachedEnd = true;
                break;
            }

            if (provisionalRowStartRx().match(line).hasMatch()) {
                flush(current);
                current = line;
            } else if (!current.isEmpty()) {
                current.append(QChar(' ')).append(line);
            }
        }
    }
    flush(current);
}

} // namespace

Estado::Estado(const QString &typeAccount)
    : Bank(QStringLiteral("BancoEstado"), typeAccount) {}

Estado::Estado(const QString &typeAccount, const QString &filePath)
    : Bank(QStringLiteral("BancoEstado"), typeAccount, filePath) {}

void Estado::readBankMovementsDebit(const QStringList &pagesText,
                                    QList<Transaction> &out)
{
    if (isProvisionalCartola(pagesText))
        parseProvisionalCartola(pagesText, nameBank, classifier, out);
    else
        parseStandardCartola(pagesText, nameBank, classifier, out);
}

void Estado::readBankMovementsCredit(const QStringList &pagesText,
                                     QList<Transaction> &out)
{
    Q_UNUSED(pagesText);
    Q_UNUSED(out);
    qWarning() << "[BancoEstado/credit] parser not implemented yet — drop a "
                  "sample PDF in files/ and we can fill in the regexes.";
}

} // namespace pdfparser
