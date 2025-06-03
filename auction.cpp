#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
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
};

std::multimap<double, Order> bid_map;
std::multimap<double, Order> offer_map;
std::unordered_map<std::string, Account> accounts_map;
std::map<double, double> matching_price_map;

int cur_cross_bid_volume = 0;
int cur_cross_offer_volume = 0;
double best_matching_price = 0;
double best_matching_amount = 0;

void calc_best_matching_price(Order &order)
{

    if (order.side == "BUY")
    {
        bid_map.emplace(order.price, order);
    }
    else if (order.side == "SELL")
    {
        offer_map.emplace(order.price, order);
    }

    if (!bid_map.empty() && !offer_map.empty() && bid_map.rbegin()->first >= offer_map.begin()->first)
    {

        double upper_bound_price = bid_map.rbegin()->first;
        double lower_bound_price = offer_map.begin()->first;

        if (best_matching_price == 0)
        {
            std::multimap<double, Order>::iterator offer_order_itr = offer_map.begin();
            std::multimap<double, Order>::reverse_iterator bid_order_itr = bid_map.rbegin();
            double start_price = offer_order_itr->first;
            double end_price = bid_order_itr->first;

            while (bid_order_itr != bid_map.rend() && bid_order_itr->first >= start_price)
            {
                matching_price_map[bid_order_itr->first] = 0;
                cur_cross_bid_volume += bid_order_itr->second.quantity;
                bid_order_itr++;
            }

            while (offer_order_itr != offer_map.end() && offer_order_itr->first <= end_price)
            {
                matching_price_map[offer_order_itr->first] = 0;
                cur_cross_offer_volume += offer_order_itr->second.quantity;
                offer_order_itr++;
            }

            double cur_max_matching_amount = 0;
            for (auto &matching_info : matching_price_map)
            {
                int matching_volume = 0;
                int available_bid_volume = 0;
                int available_offer_volume = 0;
                double matching_price = matching_info.first;

                std::multimap<double, Order>::reverse_iterator bd_itr = bid_map.rbegin();
                std::multimap<double, Order>::iterator of_itr = offer_map.begin();

                while (bd_itr != bid_map.rend() && bd_itr->first >= matching_price)
                {
                    available_bid_volume += bd_itr->second.quantity;
                    ++bd_itr;
                }

                while (of_itr != offer_map.end() && of_itr->first <= matching_price)
                {
                    available_offer_volume += of_itr->second.quantity;
                    ++of_itr;
                }

                matching_volume = std::min(available_bid_volume, available_offer_volume);
                double matching_amount = matching_volume * matching_price;
                matching_price_map[matching_price] = matching_amount;

                if (matching_amount >= cur_max_matching_amount)
                {
                    best_matching_price = matching_price;
                    best_matching_amount = matching_amount;
                }
            }
        }
        else if (order.price <= upper_bound_price && order.price >= lower_bound_price)
        {

            if (matching_price_map.find(order.price) == matching_price_map.end())
            {
                matching_price_map[order.price] = 0;
            }

            if (order.price >= best_matching_price)
            {
                if (order.side == "SELL"&&cur_cross_offer_volume >= cur_cross_bid_volume)
                {
                    cur_cross_offer_volume += order.quantity;
                    return;
                }

                if(order.price > best_matching_price){
                    return;
                }

                std::map<double, double>::iterator itr = matching_price_map.find(best_matching_price);
                while (itr != matching_price_map.end() && itr->first < order.price)
                {
                    int matching_volume = 0;
                    int available_bid_volume = 0;
                    int available_offer_volume = 0;
                    double matching_price = itr->first;

                    std::multimap<double, Order>::reverse_iterator bd_itr = bid_map.rbegin();
                    std::multimap<double, Order>::iterator of_itr = offer_map.begin();

                    while (bd_itr != bid_map.rend() && bd_itr->first >= matching_price)
                    {
                        available_bid_volume += bd_itr->second.quantity;
                        ++bd_itr;
                    }

                    while (of_itr != offer_map.end() && of_itr->first <= matching_price)
                    {
                        available_offer_volume += of_itr->second.quantity;
                        ++of_itr;
                    }

                    matching_volume = std::min(available_bid_volume, available_offer_volume);
                    double matching_amount = matching_volume * matching_price;
                    matching_price_map[matching_price] = matching_amount;

                    if (matching_amount >= matching_price_map[best_matching_price])
                    {
                        best_matching_price = matching_price;
                        best_matching_amount = matching_amount;
                    }

                    itr++;
                }
            }
            else
            {
                if (order.side == "BUY" && cur_cross_bid_volume >= cur_cross_offer_volume)
                {
                    cur_cross_bid_volume += order.quantity;
                    return;
                }

                std::map<double, double>::iterator itr = matching_price_map.find(best_matching_price);
                while (itr != matching_price_map.begin() && itr->first > best_matching_price)
                {
                    int matching_volume = 0;
                    int available_bid_volume = 0;
                    int available_offer_volume = 0;
                    double matching_price = itr->first;

                    std::multimap<double, Order>::reverse_iterator bd_itr = bid_map.rbegin();
                    std::multimap<double, Order>::iterator of_itr = offer_map.begin();

                    while (bd_itr != bid_map.rend() && bd_itr->first >= matching_price)
                    {
                        available_bid_volume += bd_itr->second.quantity;
                        ++bd_itr;
                    }

                    while (of_itr != offer_map.end() && of_itr->first <= matching_price)
                    {
                        available_offer_volume += of_itr->second.quantity;
                        ++of_itr;
                    }

                    matching_volume = std::min(available_bid_volume, available_offer_volume);
                    double matching_amount = matching_volume * matching_price;
                    matching_price_map[matching_price] = matching_amount;

                    if (matching_amount > matching_price_map[best_matching_price])
                    {
                        best_matching_price = matching_price;
                        best_matching_amount = matching_amount;
                    }
                }
            }
        }
    }
}

void matching(double best_matching_price)
{
    std::multimap<double, Order>::iterator offer_order_itr = offer_map.begin();
    std::multimap<double, Order>::iterator bid_order_itr = --bid_map.end();

    std::vector<std::multimap<double, Order>::iterator> offer_orders_vec;
    std::vector<std::multimap<double, Order>::iterator> bid_orders_vec;

    int available_bid_volume = 0;
    int available_offer_volume = 0;

    while (offer_order_itr != offer_map.end() && offer_order_itr->first <= best_matching_price)
    {
        available_offer_volume += offer_order_itr->second.quantity;
        offer_orders_vec.push_back(offer_order_itr);
        offer_order_itr++;
    }

    while (bid_order_itr != bid_map.begin() && bid_order_itr->first >= best_matching_price)
    {
        available_bid_volume += bid_order_itr->second.quantity;
        bid_orders_vec.push_back(bid_order_itr);
        bid_order_itr--;
    }

    if (bid_order_itr == bid_map.begin() && bid_order_itr->first >= best_matching_price)
    {
        available_bid_volume += bid_order_itr->second.quantity;
        bid_orders_vec.push_back(bid_order_itr);
    }

    if (available_bid_volume == 0 || available_offer_volume == 0)
    {
        return;
    }

    int matching_volume = std::min(available_bid_volume, available_offer_volume);

    sort(offer_orders_vec.begin(), offer_orders_vec.end(),
         [](const auto &order1, const auto &order2)
         { 
             if(order1->second.price != order2->second.price)
                 return order1->second.price > order2->second.price;
             else if(order1->second. quantity!= order2->second.quantity)
                 return order1->second.quantity > order2->second.quantity;
             else
                 return order1->second.timestamp < order2->second.timestamp; });

    sort(bid_orders_vec.begin(), bid_orders_vec.end(),
         [](const auto &order1, const auto &order2)
         { 
             if(order1->second.price != order2->second.price)
                 return order1->second.price > order2->second.price;
             else if(order1->second.quantity != order2->second.quantity)
                 return order1->second.quantity > order2->second.quantity;
             else
                 return order1->second.timestamp < order2->second.timestamp; });

    int available_offer_volume_count = matching_volume;
    int available_bid_volume_count = matching_volume;
    for (auto &of_itr : offer_orders_vec)
    {
        if (of_itr->second.quantity < available_offer_volume_count)
        {
            accounts_map[of_itr->second.name].position -= of_itr->second.quantity;
            accounts_map[of_itr->second.name].income += of_itr->second.quantity * best_matching_price;
            available_offer_volume_count -= of_itr->second.quantity;
            offer_map.erase(of_itr);
        }
        else
        {
            accounts_map[of_itr->second.name].position -= available_offer_volume_count;
            accounts_map[of_itr->second.name].income += available_offer_volume_count * best_matching_price;

            if (of_itr->second.quantity == available_offer_volume_count)
            {
                offer_map.erase(of_itr);
            }

            available_offer_volume_count = 0;
            break;
        }
    }

    for (auto &bd_itr : bid_orders_vec)
    {
        if (bd_itr->second.quantity < available_bid_volume_count)
        {
            accounts_map[bd_itr->second.name].position += bd_itr->second.quantity;
            accounts_map[bd_itr->second.name].expense += bd_itr->second.quantity * best_matching_price;
            available_bid_volume_count -= bd_itr->second.quantity;
            bid_map.erase(bd_itr);
        }
        else
        {
            accounts_map[bd_itr->second.name].position += available_bid_volume_count;
            accounts_map[bd_itr->second.name].expense += available_bid_volume_count * best_matching_price;

            if (bd_itr->second.quantity == available_bid_volume_count)
            {
                bid_map.erase(bd_itr);
            }

            available_bid_volume_count = 0;
            break;
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

        calc_best_matching_price(order);
        // create account for everyone
        if (accounts_map.find(order.name) == accounts_map.end())
        {
            accounts_map.emplace(order.name, Account{order.name});
        }
    }

    matching(best_matching_price);
    std::cout << "Best matching price: " << best_matching_price << std::endl;
    std::cout << "Best matching amount: " << best_matching_amount << std::endl;

    for(const auto &matching_info : matching_price_map)
    {
        std::cout << "Matching Price: " << matching_info.first
                  << ", Amount: " << matching_info.second << std::endl;
    }

    for (const auto &account : accounts_map)
    {
        std::cout << "Account: " << account.first
                  << ", Position: " << account.second.position
                  << ", Income: " << account.second.income
                  << ", Expense: " << account.second.expense
                  << ", benefit: "
                  << account.second.income - account.second.expense + account.second.position * best_matching_price << std::endl;
    }

    file.close();
    return 0;
}
