#include <iostream>
#include <string>
#include <vector>

namespace shop
{
struct Item
{
    std::string name;
    int quantity;  // how many are in the cart
    double unit_price;
};

class Cart
{
  public:
    void add(const Item& item)
    {
        items_.push_back(item);
    }

    int count() const
    {
        int total = 0;
        for (const Item& item : items_)
        {
            total += item.quantity;
        }
        return total;
    }

    const Item* find(const std::string& name) const
    {
        for (const Item& item : items_)
        {
            if (item.name == name)
                return &item;
        }
        return nullptr;
    }

    double total_price(double discount_rate, bool include_shipping, double shipping_cost,
                       const std::string& currency_label) const
    {
        double sum = 0.0;
        for (const Item& item : items_)
            sum += item.quantity * item.unit_price;
        sum = sum * (1.0 - discount_rate);
        if (include_shipping)
        {
            sum += shipping_cost;
        }
        (void)currency_label;
        return sum;
    }

  private:
    std::vector<Item> items_;
};
}  // namespace shop

int main()
{
    shop::Cart cart;
    cart.add({"bolt", 3, 0.25});
    cart.add({"nut", 5, 0.10});
    const shop::Item* bolt = cart.find("bolt");
    if (bolt != nullptr)
        std::cout << "bolts: " << bolt->quantity << "\n";
    std::cout << "items in the cart: " << cart.count()
              << ", total with a ten percent discount and shipping: "
              << cart.total_price(0.10, true, 4.50, "EUR") << "\n";
    return 0;
}
