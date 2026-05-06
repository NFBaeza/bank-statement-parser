#ifndef BICE_H
#define BICE_H

#include "bank.h"

class BICE : public Bank
{
public:
    explicit BICE(const QString &typeAccount);
    BICE(const QString &typeAccount, const QString &filePath);

protected:
    void readBankMovementsCredit(const QStringList &pagesText,
                                 QList<Transaction> &out) override;
    void readBankMovementsDebit (const QStringList &pagesText,
                                 QList<Transaction> &out) override;

private:
    static double parseClpAmount(const QString &raw);
};

#endif // BICE_H