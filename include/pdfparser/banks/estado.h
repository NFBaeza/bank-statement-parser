#ifndef PDFPARSER_ESTADO_H
#define PDFPARSER_ESTADO_H

#include "bank.h"

namespace pdfparser {

class Estado : public Bank
{
public:
    explicit Estado(const QString &typeAccount);
    Estado(const QString &typeAccount, const QString &filePath);

protected:
    void readBankMovementsCredit(const QStringList &pagesText,
                                 QList<Transaction> &out) override;
    void readBankMovementsDebit (const QStringList &pagesText,
                                 QList<Transaction> &out) override;
};

} // namespace pdfparser

#endif // PDFPARSER_ESTADO_H
