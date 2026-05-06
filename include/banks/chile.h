#ifndef CHILE_H
#define CHILE_H

#include "bank.h"

class Chile : public Bank
{
public:
    explicit Chile(const QString &typeAccount);
    Chile(const QString &typeAccount, const QString &filePath);

protected:
    void readBankMovementsCredit(const QStringList &pagesText,
                                 QList<Transaction> &out) override;
    void readBankMovementsDebit (const QStringList &pagesText,
                                 QList<Transaction> &out) override;
};

#endif // CHILE_H