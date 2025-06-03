#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <numeric>
#include <map>
#include <unordered_map>

struct Order
{
    int order_id;
    std::string name;
    double price;
    int quantity;
    int timestamp;
    std::string side;
};

struct Account
{
    std::string name;
    int position;
    double income;
    double expense;
    int volume;
    std::vector<int> historical_net_positions;
};

std::multimap<double, Order> bid_map;
std::multimap<double, Order> offer_map;
std::unordered_map<std::string, Account> accounts_map;
double current_price = 0;

void matching(Order &order)
{
    if (order.side == "BUY")
    {
        if (offer_map.empty())
        {
            bid_map.emplace(order.price, order);
            return;
        }

        auto itr = offer_map.begin();
        if (itr->first > order.price)
        {
            bid_map.emplace(order.price, order);
            return;
        }

        std::vector<std::multimap<double, Order>::iterator> orders_vec;
        int quntity_count = 0;

        while (itr != offer_map.end() && itr->first <= order.price && quntity_count < order.quantity)
        {
            quntity_count += itr->second.quantity;
            orders_vec.push_back(itr);
            ++itr;
        }

        // sort the orders based on price, timestamp and quantity
        sort(orders_vec.begin(), orders_vec.end(),
             [](const auto &order1, const auto &order2)
             { 
                    if(order1->second.price != order2->second.price)
                        return order1->second.price < order2->second.price;
                    else if(order1->second.timestamp != order2->second.timestamp)
                        return order1->second.timestamp < order2->second.timestamp;
                    else
                        return order1->second.quantity > order2->second.quantity; });

        auto &buyer_account = accounts_map[order.name];
        for (auto &offer_order_itr : orders_vec)
        {
            auto &seller_account = accounts_map[offer_order_itr->second.name];

            if (offer_order_itr->second.quantity > order.quantity)
            {
                offer_order_itr->second.quantity -= order.quantity;
                order.quantity = 0;

                buyer_account.position += order.quantity;
                buyer_account.expense += order.quantity * offer_order_itr->second.price;
                buyer_account.volume += order.quantity;
                buyer_account.historical_net_positions.push_back(buyer_account.position + order.quantity);

                seller_account.position -= order.quantity;
                seller_account.income += order.quantity * offer_order_itr->second.price;
                seller_account.volume += order.quantity;
                seller_account.historical_net_positions.push_back(seller_account.position - order.quantity);

                current_price = offer_order_itr->second.price;
                break;
            }
            else if (offer_order_itr->second.quantity == order.quantity)
            {
                order.quantity = 0;

                buyer_account.position += order.quantity;
                buyer_account.expense += order.quantity * offer_order_itr->second.price;
                buyer_account.volume += order.quantity;
                buyer_account.historical_net_positions.push_back(buyer_account.position + order.quantity);

                seller_account.position -= order.quantity;
                seller_account.income += order.quantity * offer_order_itr->second.price;
                seller_account.volume += order.quantity;
                seller_account.historical_net_positions.push_back(seller_account.position - order.quantity);

                current_price = offer_order_itr->second.price;
                // remove consumed offer order from offer map

                offer_map.erase(offer_order_itr);
                break;
            }
            else
            {
                buyer_account.position += offer_order_itr->second.quantity;
                buyer_account.expense += offer_order_itr->second.quantity * offer_order_itr->second.price;
                buyer_account.volume += offer_order_itr->second.quantity;
                buyer_account.historical_net_positions.push_back(buyer_account.position + offer_order_itr->second.quantity);

                seller_account.position -= offer_order_itr->second.quantity;
                seller_account.income += offer_order_itr->second.quantity * offer_order_itr->second.price;
                seller_account.volume += offer_order_itr->second.quantity;
                seller_account.historical_net_positions.push_back(seller_account.position - offer_order_itr->second.quantity);

                order.quantity -= offer_order_itr->second.quantity;
                current_price = offer_order_itr->second.price;
                // remove consumed offer order from offer map
                offer_map.erase(offer_order_itr);
            }
        }

        if (order.quantity > 0)
        {
            bid_map.emplace(order.price, order);
        }
    }
    else if (order.side == "SELL")
    {
        if (bid_map.empty())
        {
            offer_map.emplace(order.price, order);
            return;
        }
        auto itr = bid_map.end();
        --itr;
        if (itr->first < order.price)
        {
            offer_map.emplace(order.price, order);
            return;
        }
        std::vector<std::multimap<double, Order>::iterator> orders_vec;
        int quntity_count = 0;
        while (itr != bid_map.begin() && itr->first >= order.price && quntity_count < order.quantity)
        {
            quntity_count += itr->second.quantity;
            orders_vec.push_back(itr);
            --itr;
        }

        if (itr == bid_map.begin() && itr->first >= order.price && quntity_count < order.quantity)
        {
            orders_vec.push_back(itr);
        }

        // sort the orders based on price, timestamp and quantity
        sort(orders_vec.begin(), orders_vec.end(),
             [](const auto &order1, const auto &order2)
             { 
                    if(order1->second.price != order2->second.price)
                        return order1->second.price > order2->second.price;
                    else if(order1->second.timestamp != order2->second.timestamp)
                        return order1->second.timestamp < order2->second.timestamp;
                    else
                        return order1->second.quantity > order2->second.quantity; });

        auto &seller_account = accounts_map[order.name];
        for (auto &bid_order_itr : orders_vec)
        {
            auto &buyer_account = accounts_map[bid_order_itr->second.name];
            if (bid_order_itr->second.quantity > order.quantity)
            {
                order.quantity = 0;
                bid_order_itr->second.quantity -= order.quantity;

                seller_account.position -= order.quantity;
                seller_account.income += order.quantity * bid_order_itr->second.price;
                seller_account.volume += order.quantity;
                seller_account.historical_net_positions.push_back(seller_account.position - order.quantity);

                buyer_account.position += order.quantity;
                buyer_account.expense += order.quantity * bid_order_itr->second.price;
                buyer_account.volume += order.quantity;
                buyer_account.historical_net_positions.push_back(buyer_account.position + order.quantity);

                current_price = bid_order_itr->second.price;
                break;
            }
            else if (bid_order_itr->second.quantity == order.quantity)
            {
                order.quantity = 0;

                seller_account.position -= order.quantity;
                seller_account.income += order.quantity * bid_order_itr->second.price;
                seller_account.volume += order.quantity;
                seller_account.historical_net_positions.push_back(seller_account.position - order.quantity);

                buyer_account.position += order.quantity;
                buyer_account.expense += order.quantity * bid_order_itr->second.price;
                buyer_account.volume += order.quantity;
                buyer_account.historical_net_positions.push_back(buyer_account.position + order.quantity);

                current_price = bid_order_itr->second.price;
                // remove consumed bid order from bid map
                bid_map.erase(bid_order_itr);
                break;
            }
            else
            {
                seller_account.position -= bid_order_itr->second.quantity;
                accounts_map[order.name].income += bid_order_itr->second.quantity * bid_order_itr->second.price;
                seller_account.volume += bid_order_itr->second.quantity;
                seller_account.historical_net_positions.push_back(seller_account.position - bid_order_itr->second.quantity);

                buyer_account.position += bid_order_itr->second.quantity;
                buyer_account.expense += bid_order_itr->second.quantity * bid_order_itr->second.price;
                buyer_account.volume += bid_order_itr->second.quantity;
                buyer_account.historical_net_positions.push_back(buyer_account.position + bid_order_itr->second.quantity);

                order.quantity -= bid_order_itr->second.quantity;

                current_price = bid_order_itr->second.price;
                // remove consumed bid order from bid map
                bid_map.erase(bid_order_itr);
            }
        }

        if (order.quantity > 0)
        {
            offer_map.emplace(order.price, order);
        }
    }
}

int main()
{
    // Read orders from csv file
    std::ifstream file("orders.csv");
    if (!file.is_open())
    {
        std::cerr << "failed to open orders.csv" << std::endl;
        return 0;
    }

    std::string order_str;
    while (std::getline(file, order_str))
    {
        std::stringstream ss(order_str);
        Order order;

        // Parse data
        ss >> order.order_id;
        ss.ignore(1);

        std::getline(ss, order.name, ',');
        ss >> order.price;
        ss.ignore(1);

        ss >> order.quantity;
        ss.ignore(1);

        ss >> order.timestamp;
        ss.ignore(1);

        std::getline(ss, order.side, '\r');

        // create account for everyone
        if (accounts_map.find(order.name) == accounts_map.end())
        {
            accounts_map.emplace(order.name, Account{order.name});
        }

        matching(order);
    }

    // Output accounts information

    std::cout << std::left << std::setw(10) << "Name"
              << std::setw(8) << "Pos"
              << std::setw(10) << "Amount"
              << std::setw(10) << "Volume"
              << std::setw(10) << "Profits"
              << std::setw(10) << "max"
              << std::setw(10) << "min"
              << std::setw(10) << "average"
              << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto &account_pair : accounts_map)
    {
        auto &account = account_pair.second;
        std::cout << std::left << std::setw(10) << account_pair.first;
        if (account.position > 0)
            std::cout << std::setw(8) << " L " << std::setw(10) << account.position;
        else
            std::cout << std::setw(8) << " S " << std::setw(10) << -account.position;
        std::cout << std::setw(10) << account.volume;
        std::cout << std::setw(10) << account.income - account.expense + account.position * current_price;
        if (!account.historical_net_positions.empty())
        {
            auto max_position = *std::max_element(account.historical_net_positions.begin(), account.historical_net_positions.end());
            auto min_position = *std::min_element(account.historical_net_positions.begin(), account.historical_net_positions.end());
            double average_position = std::accumulate(account.historical_net_positions.begin(), account.historical_net_positions.end(), 0.0) / account.historical_net_positions.size();
            std::cout << std::setw(10) << max_position;
            std::cout << std::setw(10) << min_position;
            std::cout << std::setw(10) << average_position;
        }
        else
        {
            std::cout << std::setw(10) << "N/A";
            std::cout << std::setw(10) << "N/A";
            std::cout << std::setw(10) << "N/A";
        }

        std::cout << std::endl;
    }

    file.close();
    return 0;
}
