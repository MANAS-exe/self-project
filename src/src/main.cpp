#include "Database.h"
#include "ApiClient.h"
#include "Portfolio.h"
#include "CLI.h"
#include <iostream>

int main() {
    try {
        Database db("portfolio.db");
        db.open();
        db.migrate();
        db.seed_sample_stocks();

        ApiClient api; // uses hardcoded API key
        Portfolio pf(db, &api);

        CLI cli(pf);
        cli.run();

        db.close();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }
}
